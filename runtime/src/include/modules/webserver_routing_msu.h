#ifndef __WEBSERVER_ROUTING_MSU_H__
#define __WEBSERVER_ROUTING_MSU_H__
#include "generic_msu.h"
#include "routing.h"
#include "modules/webserver/connection-handler.h"

struct webserver_state {
    int fd;
    int allocated;
    uint32_t ip_address;
    struct connection_state conn_state;
};

struct webserver_queue_data {
    int fd;
    int allocated;
    uint32_t ip_address;
};

const struct msu_type WEBSERVER_ROUTING_MSU_TYPE;
struct generic_msu *webserver_routing_instance();
struct msu_endpoint *webserver_routing_endpoint();

#endif
