#ifndef ROUTING_TESTING_H_
#define ROUTING_TESTING_H_

#include "routing.h"

struct routing_table *init_routing_table(int route_id, int type_id);

int add_routing_table_entry(struct routing_table *table,
                            struct msu_endpoint *dest, uint32_t key);

#define ADD_ROUTE_TO_MSU(source_msu, dst_msu, local_rt_id, route_id) \
    do { \
        struct msu_endpoint dst_msu ##__EP__ = { \
            .id = dst_msu.id, \
            .locality = MSU_IS_LOCAL, \
            .runtime_id = local_rt_id, \
            .queue = &dst_msu.queue \
        }; \
        source_msu.routes.n_routes += 1; \
        source_msu.routes.routes = realloc(source_msu.routes.routes, \
                sizeof(struct routing_table*) * source_msu.routes.n_routes); \
        source_msu.routes.routes[source_msu.routes.n_routes - 1] = \
                init_routing_table(route_id, dst_msu.type->id); \
        add_routing_table_entry(source_msu.routes.routes[source_msu.routes.n_routes - 1], &(dst_msu ##__EP__), 1);\
    } while (0);
 
#endif
