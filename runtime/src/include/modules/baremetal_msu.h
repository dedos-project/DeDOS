#ifndef BAREMETAL_MSU_H
#define BAREMETAL_MSU_H
#include "generic_msu.h"

/** Holds data to be delivered to a baremetal MSU */
struct baremetal_msu_data_payload {
    uint64_t int_data;
};


const struct msu_type BAREMETAL_MSU_TYPE;

#endif
