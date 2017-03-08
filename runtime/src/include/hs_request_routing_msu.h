#pragma once

#include "generic_msu.h"
#include "flow_table.h"

struct routing_queue_item
{
    int src_msu_id;
    struct pico_frame *f;
};

msu_type_t HS_REQUEST_ROUTING_MSU_TYPE;

