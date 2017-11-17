/**
 * @file worker_thread.c
 *
 * Threads that hold MSUs
 */
#include "worker_thread.h"
#include "msu_type.h"
#include "local_msu.h"
#include "logging.h"
#include "thread_message.h"
#include "msu_message.h"
#include "controller_communication.h"

#include <stdlib.h>

/** The maximum ID that can be assigned to a worker thread */
#define MAX_DEDOS_THREAD_ID 32

/** Static struct to keep track of worker threads */
static struct worker_thread *worker_threads[MAX_DEDOS_THREAD_ID];

/** Allocates and returns a new worker thread structure */
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

void stop_all_worker_threads() {
    struct dedos_thread *d_threads[MAX_DEDOS_THREAD_ID];
    int n_threads=0;
    for (int i=0; i<MAX_DEDOS_THREAD_ID; i++) {
        if (worker_threads[i] != NULL) {
            d_threads[n_threads] = worker_threads[i]->thread;
            n_threads++;
            dedos_thread_stop(worker_threads[i]->thread);
        }
    }
    for (int i=0; i<n_threads; i++) {
        dedos_thread_join(d_threads[i]);
    }
}

/** Destroys all MSUs on a worker thread and frees the associated structure */
static void destroy_worker_thread(struct dedos_thread *thread, void *v_worker_thread) {
    struct worker_thread *wthread = v_worker_thread;
    for (int i=0; i < wthread->n_msus; i++) {
        destroy_msu(wthread->msus[i]);
    }
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

/** Gets the index in worker_thread::msus at which the msu_id resides */
static int get_msu_index(struct worker_thread *thread, int msu_id) {
    for (int i=0; i<thread->n_msus; i++) {
        if (thread->msus[i]->id == msu_id) {
            return i;
        }
    }
    return -1;
}

/** Removes the MSU at the given index from the worker_thread::msus */
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

/** Creates a new MSU on this thread based on the provided message */
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

/** Removes an MSU from this thread based on the provided messages */
static int del_msu_from_thread(struct worker_thread *thread, struct ctrl_delete_msu_msg *msg,
                               int ack_id) {
    int idx = get_msu_index(thread, msg->msu_id);
    if (idx == -1) {
        log_error("MSU %d does not exist on thread %d", msg->msu_id, thread->thread->id);
        return -1;
    }
    struct local_msu *msu = thread->msus[idx];
    if (msg->force) {
        destroy_msu(msu);
        remove_idx_from_msu_list(thread, idx);
    } else {
        int rtn = try_destroy_msu(msu);
        if (rtn == 1) {
            struct ctrl_delete_msu_msg *msg_cpy = malloc(sizeof(*msg_cpy));
            memcpy(msg_cpy, msg, sizeof(*msg));

            struct thread_msg *thread_msg = construct_thread_msg(DELETE_MSU,
                                                                 sizeof(*msg),
                                                                 msg_cpy);
            thread_msg->ack_id = ack_id;
            rtn = enqueue_thread_msg(thread_msg, &thread->thread->queue);
            if (rtn < 0) {
                log_error("Error re-enqueing delete MSU message");
            }
        } else {
            remove_idx_from_msu_list(thread, idx);
        }
    }
    return 0;
}

/** Modifies the MSU's routes, either adding or removing a route subscription */
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
            log(LOG_ROUTING_CHANGES, "Added route %d to msu %d route set", 
                msg->route_id, msg->msu_id);
            return 0;
        case DEL_ROUTE:
            rtn = rm_route_from_set(&msu->routes, msg->route_id);
            if (rtn < 0) {
                log_error("Error removing route %d from msu %d route set", msg->route_id, msg->msu_id);
                return -1;
            }
            log(LOG_ROUTING_CHANGES, "Removed route %d from msu %d route set", 
                msg->route_id, msg->msu_id);
            return 0;
        default:
            log_error("Unknown route message type: %d", msg->type);
            return -1;
    }
}

/** Checks whether the size of the message is equal to the size of the target struct */
#define CHECK_MSG_SIZE(msg, target) \
    if (msg->data_size != sizeof(target)) { \
        log_warn("Message data size does not match size" \
                 "of target type " #target ); \
        break; \
    }

/** Processes a message which has been sent to the worker thread  */
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
            rtn = del_msu_from_thread(thread, del_msg, msg->ack_id);
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
        case SEND_TO_PEER:
            log_error("Message (type %d) meant for main thread send to worker thread",
                      msg->type);
            break;
        default:
            log_error("Unknown message type %d delivered to worker thread %d",
                      msg->type, thread->thread->id);
            break;
    }
    if (msg->ack_id > 0) {
        send_ack_message(msg->ack_id, rtn == 0);
        log_warn("SENT ACK %d", msg->ack_id);
    }
    return rtn;
}

/** Default amount of time to wait before sem_trywait should return */
#define DEFAULT_WAIT_TIMEOUT_S 1

/** Returns the difference in time in seconds, t2 - t1 */
static double timediff_s(struct timespec *t1, struct timespec *t2) {
    return (double)(t2->tv_sec - t1->tv_sec) + (double)(t2->tv_nsec - t1->tv_nsec) * 1e-9;
}

/** Static structure for holding current time, so it can be returned from ::next_timeout */
static struct timespec cur_time;

/** Returns the next time at which the worker thread should exit its semaphore wait*/
static struct timespec *next_timeout(struct worker_thread *thread) {
    if (thread->timeouts == NULL) {
        return NULL;
    }
    struct timespec *time = &thread->timeouts->time;
    clock_gettime(CLOCK_REALTIME_COARSE, &cur_time);
    double diff_s = timediff_s(time, &cur_time);
    if (diff_s >= 0) {
        struct timeout_list *old = thread->timeouts;
        thread->timeouts = old->next;
        free(old);
        return &cur_time;
    }
    if (-diff_s > DEFAULT_WAIT_TIMEOUT_S) {
        return NULL;
    }
    cur_time = *time;
    struct timeout_list *old = thread->timeouts;
    thread->timeouts = old->next;
    free(old);
    return &cur_time;
}

int enqueue_worker_timeout(struct worker_thread *thread, struct timespec *interval) {
    struct timeout_list *tlist = calloc(1, sizeof(*tlist));
    clock_gettime(CLOCK_REALTIME, &tlist->time);
    tlist->time.tv_sec += interval->tv_sec;
    tlist->time.tv_nsec += interval->tv_nsec;
    if (tlist->time.tv_nsec > 1e9) {
        tlist->time.tv_nsec -= 1e9;
        tlist->time.tv_sec += 1;
    }
    if (thread->timeouts == NULL) {
        thread->timeouts = tlist;
        log(LOG_WORKER_THREAD, "Enqueued timeout to head of queue");
        return 0;
    }

    double diff = timediff_s(&tlist->time, &thread->timeouts->time);
    if (diff > 0) {
        tlist->next = thread->timeouts;
        thread->timeouts = tlist;
        log(LOG_WORKER_THREAD, "Enqueued timeout to queue");
        return 0;
    }

    struct timeout_list *last_timeout = thread->timeouts;
    while (last_timeout->next != NULL) {
       struct timespec *next_timeout = &last_timeout->next->time;
       diff = timediff_s(&tlist->time, next_timeout);
       if (diff > 0) {
           tlist->next = last_timeout->next;
           last_timeout->next = tlist;
            log(LOG_WORKER_THREAD, "Enqueued timeout to queue");
           return 0;
       }
       last_timeout = last_timeout->next;
    }
    last_timeout->next = tlist;
    tlist->next = NULL;
    log(LOG_WORKER_THREAD, "Enqueued timeout to queue");
    return 0;
}



/** The main worker thread loop. Checks for exit signal, processes messages */
static int worker_thread_loop(struct dedos_thread *thread, void *v_worker_thread) {
    log_info("Starting worker thread loop %d (%s)",
             thread->id, thread->mode == PINNED_THREAD ? "pinned" : "unpinned");

    struct worker_thread *self = v_worker_thread;

    while (!dedos_thread_should_exit(thread)) {
        // TODO: Get context switches
        if (thread_wait(thread, next_timeout(self)) != 0) {
            log_error("Error waiting on thread semaphore");
            continue;
        }
        for (int i=0; i<self->n_msus; i++) {
            log(LOG_MSU_DEQUEUES, "Attempting to dequeue from msu %d (thread %d)",
                       self->msus[i]->id, thread->id);
            msu_dequeue(self->msus[i]);
        }
        // FIXME: Protect read of num_msgs through mutex
        int num_msgs = thread->queue.num_msgs;
        for (int i=0; i<num_msgs; i++) {
            struct thread_msg *msg = dequeue_thread_msg(&thread->queue);
            if (msg == NULL) {
                log_error("Could not read message though queue is not empty!");
                continue;
            }
            log(LOG_THREAD_MESSAGES,"Dequeued thread message on thread %d",
                       thread->id);
            if (process_worker_thread_msg(self, msg) != 0) {
                log_error("Error processing worker thread message");
            }
            free(msg);
        }
    }
    log_info("Leaving thread %d", thread->id);
    return 0;
}

int create_worker_thread(unsigned int thread_id,
                         enum blocking_mode mode) {
    if (worker_threads[thread_id] != NULL) {
        log_error("Worker thread %u already exists", thread_id);
        return -1;
    }

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

