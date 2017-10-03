#include "worker_thread.h"
#include "msu_type.h"
#include "local_msu.h"
#include "logging.h"
#include "thread_message.h"
#include "msu_message.h"
#include "main_thread.h"

#include <stdlib.h>

#define MAX_DEDOS_THREAD_ID 16
static struct worker_thread *worker_threads[MAX_DEDOS_THREAD_ID];

static void *init_worker_thread(struct dedos_thread *thread) {
    struct worker_thread *worker = calloc(1, sizeof(*worker));
    if (worker == NULL) {
        log_error("Error allocating worker thread");
        return NULL;
    }
    worker->thread = thread;
    worker->exit_signal = 0;
    worker_threads[thread->id] = worker;
    return worker;
}

void stop_worker_thread(struct worker_thread *thread) {
    log_info("Signaling thread %d to exit", thread->thread->id);
    pthread_mutex_lock(&thread->exit_lock);
    thread->exit_signal = 1;
    pthread_mutex_unlock(&thread->exit_lock);
}

void stop_all_worker_threads() {
    struct dedos_thread *d_threads[MAX_DEDOS_THREAD_ID];
    int n_threads=0;
    for (int i=0; i<MAX_DEDOS_THREAD_ID; i++) {
        if (worker_threads[i] != NULL) {
            d_threads[n_threads] = worker_threads[i]->thread;
            n_threads++;
            stop_worker_thread(worker_threads[i]);
        }
    }
    stop_main_thread();
    for (int i=0; i<n_threads; i++) {
        dedos_thread_join(d_threads[i]);
    }
    join_main_thread();
}

static inline int should_exit(struct worker_thread *thread) {
    pthread_mutex_lock(&thread->exit_lock);
    int exit_signal = thread->exit_signal;
    pthread_mutex_unlock(&thread->exit_lock);
    return exit_signal;
}

static void destroy_worker_thread(struct dedos_thread *thread, void *v_worker_thread) {
    worker_threads[thread->id] = NULL;
    free(v_worker_thread);
}

struct worker_thread *get_worker_thread(int id) {
    if (id > MAX_DEDOS_THREAD_ID) {
        log_error("Error: ID higher than maximum thread ID: %d > %d", id, MAX_DEDOS_THREAD_ID);
        return NULL;
    }
    return worker_threads[id];
}

static int get_msu_index(struct worker_thread *thread, int msu_id) {
    for (int i=0; i<thread->n_msus; i++) {
        if (thread->msus[i]->id == msu_id) {
            return i;
        }
    }
    return -1;
}


static int remove_idx_from_msu_list(struct worker_thread *thread, int idx) {
    if (idx >= thread->n_msus) {
        return -1;
    }
    for (int i=idx; i<thread->n_msus - 1; i++) {
        thread->msus[i] = thread->msus[i+1];
    }
    thread->msus[thread->n_msus-1] = NULL;
    thread->n_msus--;
    return 0;
}

int unregister_msu_with_thread(struct local_msu *msu) {
    int idx = get_msu_index(msu->thread, msu->id);
    if (idx == -1) {
        log_error("MSU %d does not exist on thread %d", msu->id, msu->thread->thread->id);
        return -1;
    }
    return remove_idx_from_msu_list(msu->thread, idx);
}

int register_msu_with_thread(struct local_msu *msu) {
    if (msu->thread->n_msus >= MAX_MSU_PER_THREAD) {
        log_error("Too many MSUs on thread %d", msu->thread->thread->id);
        return -1;
    }
    msu->thread->msus[msu->thread->n_msus] = msu;
    msu->thread->n_msus++;
    log(LOG_MSU_REGISTRATION, "Registered msu %d with thread", msu->id);
    return 0;
}

static int create_msu_on_thread(struct worker_thread *thread, struct ctrl_create_msu_msg *msg) {
    struct msu_type *type = get_msu_type(msg->type_id);
    if (type == NULL) {
        log_error("Failed to create MSU %d. Cannot retrieve type", msg->msu_id);
        return -1;
    }
    struct local_msu *msu = init_msu(msg->msu_id, type, thread, &msg->init_data);
    if (msu == NULL) {
        log_error("Error creating MSU %d. Not placing on thread %d",
                  msg->msu_id, thread->thread->id);
        return -1;
    }
    return 0;
}

static int del_msu_from_thread(struct worker_thread *thread, struct ctrl_delete_msu_msg *msg) {
    int idx = get_msu_index(thread, msg->msu_id);
    if (idx == -1) {
        log_error("MSU %d does not exist on thread %d", msg->msu_id, thread->thread->id);
        return -1;
    }
    struct local_msu *msu = thread->msus[idx];
    destroy_msu(msu);
    remove_idx_from_msu_list(thread, idx);
    return 0;
}

static int worker_mod_msu_route(struct worker_thread *thread, struct ctrl_msu_route_msg *msg) {
    int idx = get_msu_index(thread, msg->msu_id);
    if (idx < 0) {
        log_error("MSU %d does not exist on thread %d", msg->msu_id, thread->thread->id);
        return -1;
    }
    struct local_msu *msu = thread->msus[idx];
    int rtn;
    switch (msg->type) {
        case ADD_ROUTE:
            rtn = add_route_to_set(&msu->routes, msg->route_id);
            if (rtn < 0) {
                log_error("Error adding route %d to msu %d route set", msg->route_id, msg->msu_id);
                return -1;
            }
            return 0;
        case DEL_ROUTE:
            rtn = rm_route_from_set(&msu->routes, msg->route_id);
            if (rtn < 0) {
                log_error("Error removing route %d from msu %d route set", msg->route_id, msg->msu_id);
                return -1;
            }
            return 0;
        default:
            log_error("Unknown route message type: %d", msg->type);
            return -1;
    }
}


#define CHECK_MSG_SIZE(msg, target) \
    if (msg->data_size != sizeof(target)) { \
        log_warn("Message data size does not match size" \
                 "of target type " #target ); \
        break; \
    }

static int process_worker_thread_msg(struct worker_thread *thread, struct thread_msg *msg) {
    int rtn = -1;
    switch (msg->type) {
        case CREATE_MSU:
            CHECK_MSG_SIZE(msg, struct ctrl_create_msu_msg);
            struct ctrl_create_msu_msg *create_msg = msg->data;
            rtn = create_msu_on_thread(thread, create_msg);
            if (rtn < 0) {
                log_error("Error creating MSU");
            }
            break;
        case DELETE_MSU:
            CHECK_MSG_SIZE(msg, struct ctrl_delete_msu_msg);
            struct ctrl_delete_msu_msg *del_msg = msg->data;
            rtn = del_msu_from_thread(thread, del_msg);
            if (rtn < 0) {
                log_error("Error deleting MSU");
            }
            break;
        case MSU_ROUTE:
            CHECK_MSG_SIZE(msg, struct ctrl_msu_route_msg);
            struct ctrl_msu_route_msg *route_msg = msg->data;
            rtn = worker_mod_msu_route(thread, route_msg);
            if (rtn < 0) {
                log_error("Error modifiying MSU route");
            }
            break;
        case CONNECT_TO_RUNTIME:
        case CREATE_THREAD:
        case SEND_TO_PEER:
        case MODIFY_ROUTE:
            log_error("Message (type %d) meant for main thread send to worker thread",
                      msg->type);
            break;
        default:
            log_error("Unknown message type %d delivered to worker thread %d",
                      msg->type, thread->thread->id);
            break;
    }
    return rtn;
}

static int worker_thread_loop(struct dedos_thread *thread, void *v_worker_thread) {
    log_info("Starting worker thread loop %d (%s)",
             thread->id, thread->mode == PINNED_THREAD ? "pinned" : "unpinned");

    struct worker_thread *self = v_worker_thread;

    // TODO: Exit condition!
    while (!should_exit(self)) {
        // TODO: Get context switches
        if (thread_wait(thread, NULL) != 0) {
            log_error("Error waiting on thread semaphore");
            continue;
        }
        for (int i=0; i<self->n_msus; i++) {
            log(LOG_MSU_DEQUEUES, "Attempting to dequeue from msu %d (thread %d)",
                       self->msus[i]->id, thread->id);
            msu_dequeue(self->msus[i]);
        }
        struct thread_msg *msg = dequeue_thread_msg(&thread->queue);
        // ???: Should i be looping till no more messages?
        while (msg != NULL) {
            log(LOG_THREAD_MESSAGES,"Dequeued thread message on thread %d",
                       thread->id);
            if (process_worker_thread_msg(self, msg) != 0) {
                log_error("Error processing worker thread message");
            }
            free(msg);
            msg = dequeue_thread_msg(&thread->queue);
        }
    }
    log_info("Leaving thread %d", thread->id);
    return 0;
}

int create_worker_thread(unsigned int thread_id,
                         enum blocking_mode mode) {
    struct dedos_thread *thread = malloc(sizeof(*thread));
    if (thread == NULL) {
        log_error("Error allocating worker thread");
        return -1;
    }
    int rtn = start_dedos_thread(worker_thread_loop,
                                 init_worker_thread,
                                 destroy_worker_thread,
                                 mode,
                                 thread_id,
                                 thread);
    if (rtn < 0) {
        log_error("Error starting dedos thread %d", thread_id);
        return -1;
    }
    log(LOG_THREAD_INITS, "Created worker thread %d", thread_id);
    return 0;
}

