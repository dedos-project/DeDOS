#include "logging.h"
#include "main_thread.h"
#include "worker_thread.h"
#include "dedos_threads.h"
#include "thread_message.h"
#include "msu_type.h"
#include "controller_communication.h"
#include "ctrl_runtime_messages.h"

#include <stdlib.h>
#include <netinet/ip.h>

#define MAX_WORKER_THREADS 16
static struct dedos_thread *static_main_thread;

static void *init_main_thread(struct dedos_thread *main_thread) {
    static_main_thread = main_thread;
    return NULL;
}

static int add_worker_thread(struct ctrl_create_thread_msg *msg) {
    int id = msg->thread_id;
    int rtn = create_worker_thread(id, msg->mode);
    if (rtn < 0) {
        log_error("Error creating worker thread %d", id);
        return -1;
    }
    log(LOG_MAIN_THREAD, "Created worker thread %d", id);
    return 0;
}

static int main_thread_connect_to_runtime(struct ctrl_add_runtime_msg *msg){
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = msg->ip;
    addr.sin_port = htons(msg->port);

    int rtn = connect_to_runtime_peer(msg->runtime_id, &addr);
    if (rtn < 0) {
        log_error("Could not add runtime peer");
        return -1;
    }
    return 0;
}

static int main_thread_add_connected_runtime(struct runtime_connected_msg *msg) {
    int rtn = add_runtime_peer(msg->runtime_id, msg->fd);
    if (rtn < 0) {
        log_error("Could not add runtime peer %d (fd: %d)", msg->runtime_id, msg->fd);
        return -1;
    }
    log(LOG_MAIN_THREAD_MESSAGES, "runtime peer %d (fd: %d) added",
               msg->runtime_id, msg->fd);
    return 0;
}

static int main_thread_send_to_peer(struct send_to_peer_msg *msg) {
    int rtn = send_to_peer(msg->runtime_id, &msg->hdr, msg->data);
    if (rtn < 0) {
        log_error("Error sending message to runtime %d", msg->runtime_id);
        return -1;
    }
    return 0;
}

static int main_thread_control_route(struct ctrl_route_msg *msg) {
    int rtn;
    switch (msg->type) {
        case CREATE_ROUTE:
            rtn = init_route(msg->route_id, msg->type_id);
            if (rtn < 0) {
                log_error("Error creating new route of id %d, type %d",
                          msg->route_id, msg->type_id);
                return -1;
            }
            return 0;
        case ADD_ENDPOINT:;
            struct msu_endpoint endpoint;
            int rtn = init_msu_endpoint(msg->msu_id, msg->runtime_id, &endpoint);
            if (rtn < 0) {
                log_error("Cannot initilize runtime endpoint for adding "
                          "endpoint %d to route %d", msg->msu_id, msg->route_id);
                return -1;
            }
            rtn = add_route_endpoint(msg->route_id, endpoint, msg->key);
            if (rtn < 0) {
                log_error("Error adding endpoint %d to route %d with key %d",
                          msg->msu_id, msg->route_id, msg->key);
                return -1;
            }
            return 0;
        case DEL_ENDPOINT:
            rtn = remove_route_endpoint(msg->route_id, msg->msu_id);
            if (rtn < 0) {
                log_error("Error removing endpoint %d from route %d",
                          msg->msu_id, msg->route_id);
                return -1;
            }
            return 0;
        case MOD_ENDPOINT:
            rtn = modify_route_endpoint(msg->route_id, msg->msu_id, msg->key);
            if (rtn < 0) {
                log_error("Error modifying endpoint %d on route %d to have key %d",
                          msg->msu_id, msg->route_id, msg->key);
                return -1;
            }
            return 0;
        default:
            log_error("Unknown route control message type received: %d", msg->type);
            return -1;
    }
}

#define CHECK_MSG_SIZE(msg, target) \
    if (msg->data_size != sizeof(target)) { \
        log_warn("Message data size does not match size" \
                 "of target type " #target ); \
        break; \
    }

static int process_main_thread_msg(struct dedos_thread *main_thread,
                                   struct thread_msg *msg) {

    int rtn = -1;
    switch (msg->type) {
        case CONNECT_TO_RUNTIME:
            CHECK_MSG_SIZE(msg, struct ctrl_add_runtime_msg);
            struct ctrl_add_runtime_msg *runtime_msg = msg->data;
            rtn = main_thread_connect_to_runtime(runtime_msg);

            if (rtn < 0) {
                log_warn("Error adding runtime peer");
            }
            break;
        case RUNTIME_CONNECTED:
            CHECK_MSG_SIZE(msg, struct runtime_connected_msg);
            struct runtime_connected_msg *connect_msg = msg->data;
            rtn = main_thread_add_connected_runtime(connect_msg);

            if (rtn < 0) {
                log_warn("Error adding connected runtime");
            }
            break;
        case CREATE_THREAD:
            CHECK_MSG_SIZE(msg, struct ctrl_create_thread_msg);
            struct ctrl_create_thread_msg *create_msg = msg->data;
            rtn = add_worker_thread(create_msg);

            if (rtn < 0) {
                log_warn("Error adding worker thread");
            }
            break;
        // TODO: case DELETE_THREAD:
        case SEND_TO_PEER:
            CHECK_MSG_SIZE(msg, struct send_to_peer_msg);
            struct send_to_peer_msg *forward_msg = msg->data;
            rtn = main_thread_send_to_peer(forward_msg);

            if (rtn < 0) {
                log_warn("Error forwarding message to peer");
            }
            break;
        case MODIFY_ROUTE:
            CHECK_MSG_SIZE(msg, struct ctrl_route_msg);
            struct ctrl_route_msg *route_msg = msg->data;
            rtn = main_thread_control_route(route_msg);

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
    if (rtn > 0) {
        log(LOG_MAIN_THREAD, "Successfully processed message with type id: %d", msg->type);
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

static int main_thread_loop(struct dedos_thread *self, void UNUSED *init_data) {

    struct timespec elapsed;
    struct timespec timeout_abs;
    clock_gettime(CLOCK_REALTIME, &timeout_abs);
    while (1) {
        int rtn = thread_wait(self, &timeout_abs);
        if (rtn < 0) {
            log_error("Error waiting on main thread semaphore");
            return -1;
        }

        rtn = check_main_thread_queue(self);
        if (rtn != 0) {
            log_info("Breaking from main runtime thread loop "
                     "due to thread queue");
            break;
        }

        clock_gettime(CLOCK_REALTIME, &elapsed);
        if (elapsed.tv_sec >= timeout_abs.tv_sec) {
            send_stats_to_controller();
            clock_gettime(CLOCK_REALTIME, &timeout_abs);
            timeout_abs.tv_sec += STAT_REPORTING_DURATION_S;

        }
    }
    // TODO: destroy_runtime();
    return 0;
}

int enqueue_to_main_thread(struct thread_msg *msg) {
    int rtn = enqueue_thread_msg(msg, &static_main_thread->queue);
    if (rtn < 0) {
        log_error("Error enqueuing message %p to main thread", msg);
        return -1;
    }
    log(MAIN_THREAD, "Enqueued message %p to main thread queue", msg);
    return 0;
}

struct dedos_thread *start_main_thread(void) {
    struct dedos_thread *main_thread = malloc(sizeof(*main_thread));
    if (main_thread == NULL) {
        log_perror("Error allocating main thread");
        return NULL;
    }
    int rtn = start_dedos_thread(main_thread_loop,
                                 init_main_thread,
                                 NULL,
                                 UNPINNED_THREAD,
                                 MAIN_THREAD_ID,
                                 main_thread);
    if (rtn < 0) {
        log_error("Error starting dedos main thread loop");
        return NULL;
    }
    log_info("Started dedos main thread loop");
    return main_thread;
}
