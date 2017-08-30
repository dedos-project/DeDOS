#include "logging.h"
#include "main_thread.h"
#include "worker_thread.h"
#include "dedos_threads.h"
#include "thread_message.h"
#include "msu_type.h"
#include "ctrl_runtime_messages.h"

#include <stdlib.h>
#include <netinet/ip.h>

#define MAX_WORKER_THREADS 16
static struct worker_thread *worker_threads[MAX_WORKER_THREADS];
static int n_worker_threads = 0;

static int init_main_thread(struct dedos_thread *main_thread) {
    if (init_msu_types() != 0) {
        log_warn("Error initializing MSU types");
    }

    // TODO: Move out of initialization of main thread
    /*
    if (init_runtime_epoll() != 0) {
        log_error("Error initializing runtime epoll");
        return -1;
    }

    int local_port = local_runtime_port();
    if (local_port < 0) {
        log_error("Error getting local runtime port");
        return -1;
    }

    if (init_runtime_sockets(local_port) != 0) {
        log_error("Error initializing runtime sockets");
        return -1;
    }
*/
    return 0;
}

static int add_worker_thread(struct ctrl_create_thread_msg *msg, struct dedos_thread *main_thread) {
    int id = msg->thread_id;
    for (int i=0; i<n_worker_threads; i++) {
        struct worker_thread *worker = worker_threads[i];
        if (worker->thread->id == id) {
            log_error("Cannot add worker thread with ID %d. Already exists!", id);
            return -1;
        }
    }
    worker_threads[n_worker_threads] = create_worker_thread(id, msg->mode, main_thread);
    if (worker_threads[n_worker_threads] == NULL) {
        log_error("Error creating worker thread %d", id);
        return -1;
    } else {
        n_worker_threads++;
    }
    return 0;
}

// TODO: ADD RUNTIME MSG
int main_thread_add_runtime_peer(struct ctrl_add_runtime_msg *x){return 0;}

// TODO: main_thread_send_to_peer()
int main_thread_send_to_peer(struct send_to_runtime_msg *x){return 0;}

// TODO: modify_route()
int modify_route(struct ctrl_route_mod_msg *x){return 0;}

#define CHECK_MSG_SIZE(msg, target) \
    if (msg->data_size != sizeof(target)) { \
        log_warn("Message data size does not match size" \
                 "of target type " #target ); \
        break; \
    }

static int process_main_thread_msg(struct dedos_thread *main_thread,
                                   struct thread_msg *msg) {

    int rtn;
    switch (msg->type) {
        case ADD_RUNTIME:
            CHECK_MSG_SIZE(msg, struct ctrl_add_runtime_msg);
            struct ctrl_add_runtime_msg *runtime_msg = msg->data;
            rtn = main_thread_add_runtime_peer(runtime_msg);

            if (rtn < 0) {
                log_warn("Error adding runtime peer");
            }
            break;
        // TODO: case CONNECT_TO_RUNTIME
        case CREATE_THREAD:
            CHECK_MSG_SIZE(msg, struct ctrl_create_thread_msg);
            struct ctrl_create_thread_msg *create_msg = msg->data;
            rtn = add_worker_thread(create_msg, main_thread);

            if (rtn < 0) {
                log_warn("Error adding worker thread");
            }
        // TODO: case DELETE_THREAD:
        case SEND_TO_RUNTIME:
            CHECK_MSG_SIZE(msg, struct send_to_runtime_msg);
            struct send_to_runtime_msg *forward_msg = msg->data;
            rtn = main_thread_send_to_peer(forward_msg);

            if (rtn < 0) {
                log_warn("Error forwarding message to peer");
            }
            break;
        case MODIFY_ROUTE:
            CHECK_MSG_SIZE(msg, struct ctrl_route_mod_msg);
            struct ctrl_route_mod_msg *route_msg = msg->data;
            rtn = modify_route(route_msg);

            if (rtn < 0) {
                log_warn("Error modifying route");
            }
            break;
        case CREATE_MSU:
        case DELETE_MSU:
        case MSU_ROUTE:
            log_error("Message (type: %d) meant for worker thread sent to main thread",
                       msg->type);
            break;
        default:
            log_error("Unknown message type %d delivered to main thread", msg->type);
            break;
    }
    return rtn;
}

static int check_main_thread_queue(struct dedos_thread *main_thread) {

    struct thread_msg *msg = dequeue_thread_msg(&main_thread->queue);

    if (msg == NULL) {
        return 0;
    }

    int rtn = process_main_thread_msg(main_thread, msg);
    free(msg);
    if (rtn < 0) {
        log_error("Error processing thread msg");
    }
    return 0;
}

#define STAT_REPORTING_DURATION_S 1

static int main_thread_loop(struct dedos_thread *self, struct dedos_thread *main_thread) {
    if (self != main_thread) {
        log_warn("Main thread loop entered with self != main_thread");
    }

    struct timespec begin;
    clock_gettime(CLOCK_MONOTONIC, &begin);

    struct timespec elapsed;
    while (1) {
        int rtn = thread_wait(main_thread);
        if (rtn < 0) {
            log_error("Error waiting on main thread semaphore");
            return -1;
        }

        rtn = check_main_thread_queue(main_thread);
        if (rtn != 0) {
            log_info("Breaking from main runtime thread loop "
                     "due to thread queue");
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &elapsed);
        int time_spent = elapsed.tv_sec - begin.tv_sec;
        if (time_spent >= STAT_REPORTING_DURATION_S) {
            // TODO: send_stats_to_controller();
            clock_gettime(CLOCK_MONOTONIC, &begin);
        }
    }
    // TODO: destroy_runtime();
    return 0;
}

struct dedos_thread *start_main_thread(void) {
    struct dedos_thread *main_thread = malloc(sizeof(*main_thread));
    if (main_thread == NULL) {
        log_perror("Error allocating main thread");
        return NULL;
    }
    init_main_thread(main_thread);
    int rtn = start_dedos_thread(main_thread_loop, UNPINNED_THREAD,
                                 MAIN_THREAD_ID, main_thread, main_thread);
    if (rtn < 0) {
        log_error("Error starting dedos main thread loop");
        return NULL;
    }
    log_info("Started dedos main thread loop");
    return main_thread;
}
