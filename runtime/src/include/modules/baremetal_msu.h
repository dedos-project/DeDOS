#ifndef BAREMETAL_MSU_H
#define BAREMETAL_MSU_H
#include "generic_msu.h"

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
#endif
