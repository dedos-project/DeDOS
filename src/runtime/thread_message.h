#ifndef THREAD_MESSAGE_H_
#define THREAD_MESSAGE_H_
#include "runtime_communication.h"
#include "inter_runtime_messages.h"

/**
 * All messages that can be received by main thread or workers
 */
enum thread_msg_type {
    UNKNOWN_THREAD_MSG = 0,
    // For main thread
    // removed: SET_INIT_CONFIG = 11,
    // removed: GET_MSU_LIST    = 12,

    CONNECT_TO_RUNTIME = 13, // ctrl_add_runtime_msg (ctrl_runtime_messages.h)
    RUNTIME_CONNECTED  = 14, // runtime_connected_msg (below)

    CREATE_THREAD = 21, // ctrl_create_thread_msg (ctrl_runtime_messages.h)
    DELETE_THREAD = 22, // ctrl_delete_thread_msg (ctrl_runtime_messages.h)

    SEND_TO_PEER = 1000, // send_to_runtime_msg (below)

    MODIFY_ROUTE = 3000, // ctrl_route_msg (ctrl_runtime_messages.h)

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

struct send_to_peer_msg {
    unsigned int runtime_id;
    struct inter_runtime_msg_hdr hdr;
    void *data;
};

struct runtime_connected_msg {
    unsigned int runtime_id;
    int fd;
};

struct thread_msg *init_runtime_connected_thread_msg(unsigned int runtime_id,
                                                     int fd);
struct thread_msg *init_send_thread_msg(unsigned int runtime_id,
                                        unsigned int target_id,
                                        size_t data_len,
                                        void *data);

void destroy_thread_msg(struct thread_msg *msg);

struct thread_msg *construct_thread_msg(enum thread_msg_type type,
                                        ssize_t data_size, void *data);
int enqueue_thread_msg(struct thread_msg *msg, struct msg_queue *queue);
struct thread_msg *dequeue_thread_msg(struct msg_queue *queue);

#endif
