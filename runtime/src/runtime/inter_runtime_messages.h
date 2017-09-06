#ifndef INTER_RUNTIME_MESSAGES_H_
#define INTER_RUNTIME_MESSAGES_H_
#include "msu_type.h"
#include <stdlib.h>

enum inter_runtime_msg_type {
    RT_INIT,
    RT_FWD_TO_MSU,
};

/**
 * Messages to runtime from another runtime
 */
struct inter_runtime_msg_hdr {
    enum inter_runtime_msg_type type;
    unsigned int target; /**< Could be thread ID, could be MSU ID */
    size_t payload_size;
};

struct inter_runtime_init_msg {
    unsigned int origin_id;
};

#endif
