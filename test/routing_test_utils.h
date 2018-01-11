#ifndef ROUTING_TESTING_H_
#define ROUTING_TESTING_H_

#include "routing.h"
#include "runtime_dfg.h"

struct routing_table *init_routing_table(int route_id, int type_id);

int add_routing_table_entry(struct routing_table *table,
                            struct msu_endpoint *dest, uint32_t key);

#define INIT_LOCAL_RUNTIME(runtime_id) \
    struct dfg_runtime __LOCAL_RT__ = {\
        .id = runtime_id \
    }; \
    set_local_runtime(&__LOCAL_RT__);

#define ADD_ROUTE_TO_MSU(source_msu, dst_msu, local_rt_id, route_id) \
    do { \
        source_msu.routes.n_routes += 1; \
        source_msu.routes.routes = realloc(source_msu.routes.routes, \
                sizeof(struct routing_table*) * source_msu.routes.n_routes); \
        source_msu.routes.routes[source_msu.routes.n_routes - 1] = \
                init_routing_table(route_id, dst_msu.type->id); \
        struct msu_endpoint dst_msu ##__EP__ = { \
            .id = dst_msu.id, \
            .locality = MSU_IS_LOCAL, \
            .runtime_id = local_rt_id, \
            .queue = &dst_msu.queue \
        }; \
        add_routing_table_entry(source_msu.routes.routes[source_msu.routes.n_routes - 1], \
                                &(dst_msu ##__EP__), 1); \
    } while (0)

#define ADD_ROUTES_TO_MSUS(source_msus, dst_msus, local_rt_id, route_id) \
    do { \
        struct routine_table *table = init_routing_table(route_id, dst_msus[0].type->id); \
        for (int i=0; i < sizeof(source_msus) / sizeof(*source_msus); i++) { \
            source_msus[i].routes.n_routes += 1; \
            source_msus[i].routes.routes = realloc(source_msu.routes.routes, \
                    sizeof(struct routing_table*) * source_msu.routes.n_routes); \
            source_msu[i].routes.routes[source_msu.routes.n_routes - 1] = table; \
        } \
        for (int i=0; i < sizeof(dst_msus) / sizeof(*dst_msus); i++) { \
            struct msu_endpoint new_ep = { \
                .id = dst_msu.id, \
                .locality = MSU_IS_LOCAL, \
                .runtime_id = local_rt_id, \
                .queue = &dst_msu.queue \
            }; \
            add_routing_table_entry(table, &new_ep, i+1); \
        } \
    } while (0);

#define FREE_MSU_ROUTES(source_msu) \
    do { \
        for (int i=0; i < source_msu.routes.n_routes; i++) { \
            free(source_msu.routes.routes[i]); \
        } \
        free(source_msu.routes.routes); \
    } while (0);

#define FREE_MSUS_ROUTES(source_msus) \
    do { \
        for (int i=0; i < source_msus[0].routes.n_routes; i++) { \
            free(source_msus[0].routes.routes[i]) \
        } \
        for (int i=0; i < sizeof(source_msus) / sizeof(*source_msu); i++) { \
            free(source_msus[i].routes.routes); \
        } \
    } while (0)

#endif
