#ifndef ROUTING_H_
#define ROUTING_H_
#include <inttypes.h>
#include "dfg.h"
#include "message_queue.h"

struct msu_endpoint {
    int id;
    enum msu_locality locality;
    union {
        unsigned int runtime_id;
        struct msg_queue *queue;
    };
};

/**
 * Forward declaration so reads and writes have to take place through
 * API and adhere to lock
 */
struct routing_table;

struct route_set {
    int n_routes;
    struct routing_table **routes;
};

/** Initializes a new route with the given route_id and type_id.*/
int init_route(int route_id, int type_id);

/** Gets the endpoint associated with the given key in the provided route */
struct msu_endpoint *get_route_endpoint(struct routing_table *table, uint32_t key);
/** Gets the local endpoint from a routing table with the shortest queue length. */
struct msu_endpoint *get_shortest_queue_endpoint(struct routing_table *table, int n_routes, 
                                                 uint32_t key);
/** Gets endpoint witih given index in route set */
struct msu_endpoint *get_endpoint_by_index(struct routing_table *table, int index);
/** Gets endpoint with the given MSU ID in the provided route */
struct msu_endpoint *get_endpoint_by_id(struct routing_table *table, int msu_id);
/** Gets the endpoint associated with the given key in the provided route*/
struct msu_endpoint *get_route_endpoint(struct routing_table *table, uint32_t key);

/** Gets the route from the provided array of routes which has the correct type ID */
struct routing_table *get_type_from_route_set(struct route_set *routes, int type_id);

/** Adds an endpoint to the route with the given ID */
int add_route_endpoint(int route_id, struct msu_endpoint *endpoint, uint32_t key);
/** Removes destination from route with given ID */
int remove_route_endpoint(int route_id, int msu_id);
/** Modifies key associated with MSU in route */
int modify_route_endpoint(int route_id, int msu_id, uint32_t new_key);

/** Adds a route to a set of routes */
int add_route_to_set(struct route_set *set, int route_id);
/** Removes a route from a set of routes */
int rm_route_from_set(struct route_set *set, int route_id);
#endif
