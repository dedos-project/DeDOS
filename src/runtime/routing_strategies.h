#ifndef ROUTING_STRATEGIES_H_
#define ROUTING_STRATEGIES_H_
#include "msu_type.h"

int default_routing(struct msu_type *type, struct local_msu *sender,
                    struct msu_msg *msg, struct msu_endpoint *output);

int shortest_queue_route(struct msu_type *type, struct local_msu *sender,
                         struct msu_msg *msg, struct msu_endpoint *output);

int route_to_id(struct msu_type *type, struct local_msu *sender,
                int msu_id, struct msu_endpoint *output);

#endif
