#ifndef THREAD_MESSAGE_H_
#define THREAD_MESSAGE_H_
#include "runtime_communication.h"

/**
 * All messages that can be received by main thread or workers
 */
enum thread_msg_type {
    // For main thread
    // REMOVED: SET_INIT_CONFIG = 11,
    // REMOVED: GET_MSU_LIST    = 12,
    // RENAMED: RUNTIME_ENDPOINT_ADD -> ADD_RUNTIME
    // IMP: CONNECT_TO_RUNTIME (initiate, from controller)
    // IMP: vs ADD_RUNTIME (already connected, from sockets)

    ADD_RUNTIME        = 13, // ctrl_add_runtime_msg (ctrl_runtime_messages.h)
    CONNECT_TO_RUNTIME = 14, // TODO: message type, connect_to_runtime_msg
    //???: SET_DEDOS_RUNTIMES_LIST = 20
    CREATE_THREAD = 21, // ctrl_create_thread_msg (ctrl_runtime_messages.h)
    DELETE_THREAD = 22, // ctrl_delete_thread_msg (ctrl_runtime_messages.h)

    SEND_TO_RUNTIME = 1000, // send_to_runtime_msg (below)

    // TODO: SEND_TO_CONTROLLER,
    MODIFY_ROUTE = 3000,

    // For worker threads
    CREATE_MSU = 2001, // ctrl_create_msu_msg (ctrl_runtime_messages.h)
    DELETE_MSU = 2002, // ctrl_delete_msu_msg (ctrl_runtime_messages.h)
    MSU_ROUTE  = 3001, // ctrl_msu_route_msg  (ctrl_runtime_messages.h)
};

struct thread_msg {
    enum thread_msg_type type;
    ssize_t data_size;
    void *data;
};

struct send_to_runtime_msg {
    struct runtime_peer_msg_hdr header;
    unsigned int runtime_id;
    void *data;
};

struct thread_msg *construct_thread_msg(enum thread_msg_type type,
                                        ssize_t data_size, void *data);
int enqueue_thread_msg(struct thread_msg *msg, struct msg_queue *queue);
struct thread_msg *dequeue_thread_msg(struct msg_queue *queue);

#endif
