#ifndef BAREMETAL_MSU_H
#define BAREMETAL_MSU_H
#include <sys/poll.h>
#include "generic_msu.h"

#define INITIAL_ACTIVE_SOCKETS_SIZE 10

/** Internal state struct for tracking active connection and use in poll */
struct baremetal_msu_internal_state {
    int active_sockets;
    int total_array_size;
    struct pollfd *fds;
};

/** Holds data to be delivered to a baremetal MSU */
struct baremetal_msu_data_payload {
    int type;
    int socketfd;
    unsigned int int_data;
};
#define BAREMETAL_RECV_BUFFER_SIZE 64

#define NEW_ACCEPTED_CONN 1
#define READ_FROM_SOCK 2
#define FORWARD 3
#define WRITE_TO_SOCK 4

const struct msu_type BAREMETAL_MSU_TYPE;
struct generic_msu *baremetal_entry_msu;
int baremetal_msu_init_entry(struct generic_msu *self,
        struct create_msu_thread_msg_data *create_action);
void baremetal_msu_destroy_entry(struct generic_msu *self);
#endif
