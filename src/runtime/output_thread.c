#include "logging.h"
#include "worker_thread.h"
#include "dedos_threads.h"
#include "thread_message.h"
#include "msu_type.h"
#include "controller_communication.h"
#include "ctrl_runtime_messages.h"
#include "output_thread.h"

#include <stdlib.h>
#include <netinet/ip.h>

static struct dedos_thread *static_output_thread;

static void *init_output_thread(struct dedos_thread *output_thread) {
    static_output_thread = output_thread;
    return NULL;
}

static int output_thread_send_to_ctrl(struct send_to_ctrl_msg *msg) {
    int rtn = send_to_controller(&msg->hdr, msg->data);
    if (rtn < 0) {
        log_error("Error sending message to controller");
        return -1;
    }
    // TODO: Free msg? data?
    return 0;
}

static int output_thread_send_to_peer(struct send_to_peer_msg *msg) {
    int rtn = send_to_peer(msg->runtime_id, &msg->hdr, msg->data);
    if (rtn < 0) {
        log_error("Error sending message to runtime %d", msg->runtime_id);
        return -1;
    }
    return 0;
}

static int output_thread_connect_to_runtime(struct ctrl_add_runtime_msg *msg){
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

void stop_output_monitor() {
    dedos_thread_stop(static_output_thread);
}

void join_output_thread() {
    pthread_join(static_output_thread->pthread, NULL);
    free(static_output_thread);
}

#define CHECK_MSG_SIZE(msg, target) \
    if (msg->data_size != sizeof(target)) { \
        log_warn("Message data size (%d) does not match size" \
                 "of target type (%d)" #target, (int)msg->data_size , \
                 (int)sizeof(target)); \
        return -1; \
    } \

static int process_output_thread_msg(struct thread_msg *msg) {

    log(LOG_MAIN_THREAD, "processing message %p with type id: %d",
        msg, msg->type);
    int rtn = -1;
    switch (msg->type) {
        case CONNECT_TO_RUNTIME:
            CHECK_MSG_SIZE(msg, struct ctrl_add_runtime_msg);
            struct ctrl_add_runtime_msg *runtime_msg = msg->data;
            rtn = output_thread_connect_to_runtime(runtime_msg);

            if (rtn < 0) {
                log_warn("Error adding runtime peer");
            }
            break;
        case SEND_TO_PEER:
            CHECK_MSG_SIZE(msg, struct send_to_peer_msg);
            struct send_to_peer_msg *forward_msg = msg->data;
            rtn = output_thread_send_to_peer(forward_msg);

            if (rtn < 0) {
                log_warn("Error forwarding message to peer");
            }
            break;
        case SEND_TO_CTRL:
            CHECK_MSG_SIZE(msg, struct send_to_ctrl_msg);
            struct send_to_ctrl_msg *ctrl_msg = msg->data;
            rtn = output_thread_send_to_ctrl(ctrl_msg);
            if (rtn < 0) {
                log_warn("Error sending message to controller");
            }
            break;
        case CREATE_MSU:
        case DELETE_MSU:
        case MSU_ROUTE:
            log_error("Message (type: %d) meant for worker thread sent to output thread",
                       msg->type);
            break;
        default:
            log_error("Unknown message type %d delivered to output thread", msg->type);
            break;
    }
    return rtn;
}

static int check_output_thread_queue(struct dedos_thread *output_thread) {

    struct thread_msg *msg = dequeue_thread_msg(&output_thread->queue);

    if (msg == NULL) {
        return 0;
    }

    int rtn = process_output_thread_msg(msg);
    free(msg);
    if (rtn < 0) {
        log_error("Error processing thread msg");
    }
    return 0;
}

#define STAT_REPORTING_DURATION_S 1

static int output_thread_loop(struct dedos_thread *self, void UNUSED *init_data) {

    struct timespec elapsed;
    struct timespec timeout_abs;
    clock_gettime(CLOCK_REALTIME, &timeout_abs);
    while (!dedos_thread_should_exit(self)) {

        clock_gettime(CLOCK_REALTIME, &elapsed);
        if (elapsed.tv_sec >= timeout_abs.tv_sec) {
            if (send_stats_to_controller() < 0) {
                log_error("Error sending stats to controller");
            }
            log(LOG_STATS_SEND, "Sent stats");
            clock_gettime(CLOCK_REALTIME, &timeout_abs);
            timeout_abs.tv_sec += STAT_REPORTING_DURATION_S;
        }

        int rtn = thread_wait(self, &timeout_abs);
        if (rtn < 0) {
            log_error("Error waiting on output thread semaphore");
            return -1;
        }

        rtn = check_output_thread_queue(self);
        if (rtn != 0) {
            log_info("Breaking from output loop "
                     "due to thread queue");
            break;
        }
    }
    return 0;
}

int enqueue_for_output(struct thread_msg *msg) {
    int rtn = enqueue_thread_msg(msg, &static_output_thread->queue);
    if (rtn < 0) {
        log_error("Error enqueuing message %p to main thread", msg);
        return -1;
    }
    log(MAIN_THREAD, "Enqueued message %p to main thread queue", msg);
    return 0;
}

struct dedos_thread *start_output_monitor_thread(void) {
    struct dedos_thread *output_thread = malloc(sizeof(*output_thread));
    if (output_thread == NULL) {
        log_perror("Error allocating output thread");
        return NULL;
    }
    int rtn = start_dedos_thread(output_thread_loop,
                                 init_output_thread,
                                 NULL,
                                 UNPINNED_THREAD,
                                 OUTPUT_THREAD_ID,
                                 output_thread);
    if (rtn < 0) {
        log_error("Error starting output thread loop");
        return NULL;
    }
    log_info("Started output thread loop");
    return output_thread;
}

