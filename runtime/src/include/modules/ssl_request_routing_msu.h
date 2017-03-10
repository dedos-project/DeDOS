#ifndef SSL_REQUEST_ROUTING_MSU_H_
#define SSL_REQUEST_ROUTING_MSU_H_

#include "generic_msu.h"
#include "generic_msu_queue.h"
#include "generic_msu_queue_item.h"
#include "flow_table.h"

struct msu_endpoint *last_endpoint;

struct ssl_routing_queue_item
{
    int src_msu_id;
    void *data;
};

const msu_type SSL_REQUEST_ROUTING_MSU_TYPE;
local_msu *ssl_request_routing_msu;


#endif /* SSL_REQUEST_ROUTING_MSU_H_ */
