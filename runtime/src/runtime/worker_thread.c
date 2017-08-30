#include "worker_thread.h"
#include "msu_type.h"
#include "local_msu.h"
#include "logging.h"

#include <stdlib.h>

static void init_worker_thread(struct worker_thread *worker, struct dedos_thread *thread) {
    worker->thread = thread;
    worker->n_msus = 0;
}

static int get_msu_index(struct worker_thread *thread, int msu_id) {
    for (int i=0; i<thread->n_msus; i++) {
        if (thread->msus[i]->id == msu_id) {
            return i;
        }
    }
    return -1;
}

static void remove_idx_from_msu_list(struct worker_thread *thread, int idx) {
    for (int i=idx; i<thread->n_msus - 1; i++) {
        thread->msus[i] = thread->msus[i+1];
    }
    thread->n_msus--;
}

struct create_msu_msg {
    int type_id;
    int msu_id;
    struct msu_init_data *init_data;
};

static int create_msu_on_thread(struct worker_thread *thread, struct create_msu_msg *msg) {
    struct msu_type *type = get_msu_type(msg->type_id);
    if (type == NULL) {
        log_error("Failed to create MSU %d. Cannot retrieve type", msg->msu_id);
        return -1;
    }
    struct local_msu *msu = init_msu(msg->msu_id, type, thread, msg->init_data);
    if (msu == NULL) {
        log_error("Error creating MSU %d. Not placing on thread %d",
                  msg->msu_id, thread->thread->id);
        return -1;
    }
    thread->msus[thread->n_msus] = msu;
    thread->n_msus++;
    return 0;
}

struct delete_msu_msg {
    int msu_id;
};

static int del_msu_from_thread(struct worker_thread *thread, struct delete_msu_msg *msg) {
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

struct msu_route_msg {
    int msu_id;
    int route_id;
};

static int worker_add_msu_route(struct worker_thread *thread, struct msu_route_msg *msg) {
    int idx = get_msu_index(thread, msg->msu_id);
    if (idx < 0) {
        log_error("MSU %d does not exist on thread %d", msg->msu_id, thread->thread->id);
        return -1;
    }
    struct local_msu *msu = thread->msus[idx];
    int rtn = add_route_to_set(&msu->routes, msg->route_id);
    if (rtn < 0) {
        log_error("Error adding route %d to msu %d route set", msg->route_id, msg->msu_id);
        return -1;
    }
    return 0;
}

static int worker_del_msu_route(struct worker_thread *thread, struct msu_route_msg *msg) {
    int idx = get_msu_index(thread, msg->msu_id);
    if (idx < 0) {
        log_error("MSU %d does not exist on thread %d", msg->msu_id, thread->thread->id);
        return -1;
    }
    struct local_msu *msu = thread->msus[idx];
    int rtn = rm_route_from_set(&msu->routes, msg->route_id);
    if (rtn < 0) {
        log_error("Error removing route %d from msu %d route set", msg->route_id, msg->msu_id);
        return -1;
    }
    return 0;
}

#define CHECK_MSG_SIZE(msg, target) \
    if (msg->data_size != sizeof(target)) { \
        log_warn("Message data size does not match size" \
                 "of target type " #target ); \
        break; \
    }

static int process_worker_thread_msg(struct worker_thread *thread, struct thread_msg *msg) {
    int rtn;
    switch (msg->type) {
        case CREATE_MSU:
            CHECK_MSG_SIZE(msg, struct create_msu_msg);
            struct create_msu_msg *create_msg = msg->data;
            rtn = create_msu_on_thread(thread, create_msg);
            if (rtn < 0) {
                log_error("Error creating MSU");
            }
            break;
        case DELETE_MSU:
            CHECK_MSG_SIZE(msg, struct delete_msu_msg);
            struct delete_msu_msg *del_msg = msg->data;
            rtn = del_msu_from_thread(thread, del_msg);
            if (rtn < 0) {
                log_error("Error deleting MSU");
            }
            break;
        case ADD_ROUTE:
        case DEL_ROUTE:
            CHECK_MSG_SIZE(msg, struct msu_route_msg);
            struct msu_route_msg *route_msg = msg->data;
            if (msg->type == ADD_ROUTE) {
                rtn = worker_add_msu_route(thread, route_msg);
            } else {
                rtn = worker_del_msu_route(thread, route_msg);
            }
            if (rtn < 0) {
                log_error("Error modifiying MSU route");
            }
            break;
        case ADD_RUNTIME:
        case CREATE_THREAD:
        case SEND_TO_RUNTIME:
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

static int worker_thread_loop(struct dedos_thread *thread, struct dedos_thread *main_thread) {
    log_info("Starting worker thread loop %d", thread->id);

    struct worker_thread self;
    init_worker_thread(&self, thread);
    // TODO: Get context switches

    while (1) {
        // TODO: check return
        thread_wait(thread);
        for (int i=0; i<self.n_msus; i++) {
            struct msu_msg *msg = dequeue_msu_msg(&self.msus[i]->msu_queue);
            if (msg){
                msu_receive(self.msus[i], msg);
                free(msg);
            }
        }
        struct thread_msg *msg = dequeue_thread_msg(&thread->queue);
        // TODO: Should loop till no more messages?
        while (msg != NULL) {
            // TODO: Check return
            process_worker_thread_msg(&self, msg);
            free(msg);
            msg = dequeue_thread_msg(&thread->queue);
        }
    }
    log_info("Leaving thread %d", thread->id);
    return 0;
}


struct worker_thread *create_worker_thread(int thread_id,
                                           enum thread_behavior blocking,
                                           struct dedos_thread *main_thread) {
    struct worker_thread *worker_thread = malloc(sizeof(*worker_thread));
    int rtn = start_dedos_thread(worker_thread_loop, blocking,
                                 thread_id, worker_thread->thread, main_thread);
    if (rtn < 0) {
        log_error("Error starting dedos thread %d", thread_id);
        return NULL;
    }
    log_info("Created worker thread %d", thread_id);
    return worker_thread;
}

