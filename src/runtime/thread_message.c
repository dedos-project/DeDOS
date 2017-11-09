/**
 * @file thread_message.c
 *
 * Messages to be delivered to dedos_threads
 */

#include "thread_message.h"
#include "logging.h"

int enqueue_thread_msg(struct thread_msg *thread_msg, struct msg_queue *queue) {
    struct dedos_msg *msg = malloc(sizeof(*msg));
    if (msg == NULL) {
        log_error("Error allocating dedos_msg for thread_msg");
        return -1;
    }
    msg->data_size = sizeof(*thread_msg);
    msg->type = THREAD_MSG;
    msg->data = thread_msg;
    int rtn = enqueue_msg(queue, msg);
    if (rtn < 0) {
        log_error("Error enqueueing message on thread message queue");
        return -1;
    }
    log(LOG_THREAD_MESSAGES, "Enqueued thread message %p on queue %p", 
               thread_msg, queue);
    return 0;
}

struct thread_msg *dequeue_thread_msg(struct msg_queue *queue) {
    struct dedos_msg *msg = dequeue_msg(queue);
    if (msg == NULL) {
        return NULL;
    }
    if (msg->data_size != sizeof(struct thread_msg)) {
        log_error("Attempted to dequeue non-thread msg from queue");
        return NULL;
    }

    struct thread_msg *thread_msg = msg->data;
    log(LOG_THREAD_MESSAGES, "Dequeued thread message %p from queue %p", 
               thread_msg, queue);
    free(msg);
    return thread_msg;
}


struct thread_msg *construct_thread_msg(enum thread_msg_type type,
                                        ssize_t data_size, void *data) {
    struct thread_msg *msg = calloc(1, sizeof(*msg));
    if (msg == NULL) {
        log_error("Error allocating thread message");
        return NULL;
    }
    msg->type = type;
    msg->data_size = data_size;
    msg->data = data;
    log(LOG_THREAD_MESSAGES, "Constructed thread message %p of size %d",
               msg, (int)data_size);
    return msg;
}

void destroy_thread_msg(struct thread_msg *msg) {
    log(LOG_THREAD_MESSAGES, "Freeing thread message %p",
               msg);
    free(msg);
}

struct thread_msg *init_send_thread_msg(unsigned int runtime_id,
                                        unsigned int target_id,
                                        size_t data_len,
                                        void *data) {
    struct send_to_peer_msg *msg = malloc(sizeof(*msg));
    if (msg == NULL) {
        log_error("Error allocating send_to_runtime_thread_msg");
        return NULL;
    }
    msg->hdr.type = RT_FWD_TO_MSU;
    msg->hdr.target = target_id;
    msg->hdr.payload_size = data_len;
    msg->runtime_id = runtime_id;
    msg->data = data;

    struct thread_msg *thread_msg = construct_thread_msg(SEND_TO_PEER, 
                                                         sizeof(*msg), msg);
    if (thread_msg == NULL) {
        log_error("Error creating thread message for send-to-runtime message");
        return NULL;
    }
    return thread_msg;
}

