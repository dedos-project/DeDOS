#ifndef BAREMETAL_MSU_H
#define BAREMETAL_MSU_H
#include "generic_msu.h"
#include "ssl_msu.h" //for definition of READ and WRITE type

/** Holds data to be delivered to a baremetal MSU */
struct baremetal_msu_data_payload {
    int type;
    int socketfd;
    uint64_t int_data;
};


const struct msu_type BAREMETAL_MSU_TYPE;
struct generic_msu *baremetal_entry_msu;
#endif
