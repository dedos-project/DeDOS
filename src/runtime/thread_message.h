/**
 * @file thread_message.h
 *
 * Messages to be delivered to dedos_threads
 */

#ifndef THREAD_MESSAGE_H_
#define THREAD_MESSAGE_H_
#include "runtime_communication.h"
#include "inter_runtime_messages.h"
#include "rt_controller_messages.h"

/**
 * All messages that can be received by output thread or workers
 */
enum thread_msg_type {
    /** Kept unknown at 0 to catch mis-labeled messages */
    UNKNOWN_THREAD_MSG = 0,

    // For the output monitor thread
    CONNECT_TO_RUNTIME = 13, /**< payload: ctrl_add_runtime_msg (ctrl_runtime_messages.h) */
    SEND_TO_PEER = 1000, /**< payload:  send_to_runtime_msg (below) */
    SEND_TO_CTRL = 1001, /**< payload: send_to_ctrl_msg (below) */

    // For worker threads
    CREATE_MSU = 2001, /**< ctrl_create_msu_msg (ctrl_runtime_messages.h) */
    DELETE_MSU = 2002, /**< ctrl_delete_msu_msg (ctrl_runtime_messages.h) */
    MSU_ROUTE  = 3001, /**< ctrl_msu_route_msg  (ctrl_runtime_messages.h) */
};

/** A message to be delivered to a dedos_thread */
struct thread_msg {
    enum thread_msg_type type;
    int ack_id; /**< for sending acknowledgements to controller. Not implemented fully */
    ssize_t data_size;
    void *data;
};

/** For delivery to the output monitor thread, a message to be sent to a peer runtime */
struct send_to_peer_msg {
    unsigned int runtime_id; /**< The runtime ID to which the message is delivered */
    struct inter_runtime_msg_hdr hdr;
    void *data;
};

/** For delivery to output monitor thread, a message to be sent to the controller */
struct send_to_ctrl_msg {
    struct rt_controller_msg_hdr hdr;
    void *data;
};

/**
 * Initializes a send_to_peer message (::SEND_TO_PEER)
 *
 * @param runtime_id The runtime to deliver the message to
 * @param target_id The remote target (either thread ID or MSU ID)
 * @param data_len size of provided data
 * @return Newly allocated thread message on success, NULL on error
 * */
struct thread_msg *init_send_thread_msg(unsigned int runtime_id,
                                        unsigned int target_id,
                                        size_t data_len,
                                        void *data);

/** Frees a thread message */
void destroy_thread_msg(struct thread_msg *msg);

/** Allocates and initializes a thread message with the provided options */
struct thread_msg *construct_thread_msg(enum thread_msg_type type,
                                        ssize_t data_size, void *data);

/** Enqueues a dedos_msg with a thread_msg as the payload to the appropriate queue */
int enqueue_thread_msg(struct thread_msg *msg, struct msg_queue *queue);

/**
 * Dequeues a thread_msg from the message queue.
 * @returns The dequeued message or NULL if error
 */
struct thread_msg *dequeue_thread_msg(struct msg_queue *queue);

#endif
