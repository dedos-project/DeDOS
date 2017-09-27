#ifndef ROUTING_H_
#define ROUTING_H_
#include <inttypes.h>
#include "dfg.h"
#include "message_queue.h"

struct msu_endpoint {
    int id;
    enum msu_locality locality;
    unsigned int runtime_id;
    struct msg_queue *queue; // Only filled if MSU_IS_LOCAL
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
int get_route_endpoint(struct routing_table *table, uint32_t key, 
                       struct msu_endpoint *endpoint);
/** Gets the local endpoint from a routing table with the shortest queue length. */
int get_shortest_queue_endpoint(struct routing_table *table,
                                uint32_t key, struct msu_endpoint *endpoint);
/** Gets endpoint witih given index in route set */
int get_endpoint_by_index(struct routing_table *table, int index, 
                          struct msu_endpoint *endpoint);
/** Gets endpoint with the given MSU ID in the provided route */
int get_endpoint_by_id(struct routing_table *table, int msu_id,
                       struct msu_endpoint *endpoint);

int get_endpoints_by_runtime_id(struct routing_table *table, int runtime_id,
                                struct msu_endpoint *endpoints, int n_endpoints);

/** Gets the route from the provided array of routes which has the correct type ID */
struct routing_table *get_type_from_route_set(struct route_set *routes, int type_id);

int get_n_endpoints(struct routing_table *table);


/** Adds an endpoint to the route with the given ID. 
 * Note that endpoints are added by value, not by reference
 */
int add_route_endpoint(int route_id, struct msu_endpoint endpoint, uint32_t key);
/** Removes destination from route with given ID */
int remove_route_endpoint(int route_id, int msu_id);
/** Modifies key associated with MSU in route */
int modify_route_endpoint(int route_id, int msu_id, uint32_t new_key);

/** Adds a route to a set of routes */
int add_route_to_set(struct route_set *set, int route_id);
/** Removes a route from a set of routes */
int rm_route_from_set(struct route_set *set, int route_id);

/** Initializes an endpoint structure to point to the relevant msu. */
int init_msu_endpoint(int msu_id, int runtime_id, struct msu_endpoint *endpoint);
#endif
