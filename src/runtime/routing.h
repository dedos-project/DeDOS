/**
 * @file routing.h
 *
 * Functions for routing MSU messages between MSUs
 */
#ifndef ROUTING_H_
#define ROUTING_H_
#include <inttypes.h>
#include "dfg.h"
#include "message_queue.h"

/** An endpoint to which an msu_msg can be delivered */
struct msu_endpoint {
    /** A unque identifier for the endpoint (msu ID) */
    int id;
    /** Whether the endpoint is on the local machine or remote */
    enum msu_locality locality;
    /** The ID for the route used to get to the endpoint */
    unsigned int route_id;
    /** The ID for the runtime on which the endpoint resides */
    unsigned int runtime_id;
    /** if msu_endpoint::locality == ::MSU_IS_LOCAL, the queue for the msu endpoint */
    struct msg_queue *queue;
};

/**
 * Forward declaration so reads and writes have to take place through
 * API and adhere to lock
 */
struct routing_table;

/**
 * The publicly accessible copy of the routing table.
 * A list of defined routing tables, which can be queried with the functions below
 */
struct route_set {
    int n_routes;
    struct routing_table **routes;
};

/**
 * Initializes a new route with the given route_id and type_id.
 * @param route_id The ID to assign to the new route
 * @param type_id The Type ID to assign to the new route
 * @return 0 on success, -1 on error
 */
int init_route(int route_id, int type_id);

/**
 * Gets the endpoint associated with the given key in the provided route
 * @param route Route to search for key
 * @param key The key with which to search the route set
 * @pararm endpoint Points to the appropraite endpoint on success
 * @return 0 on success, -1 on error
 */
int get_route_endpoint(struct routing_table *table, uint32_t key,
                       struct msu_endpoint *endpoint);

/**
 * Gets the local endpoint from a route with the shortest queue length.
 * Ignores enpoints on remote runtimes.
 * @param table The route to search for the shortest queue length
 * @param key Used as a tiebreaker in the case of multiple MSUs with same queue length
 * @param endpoint Points to appropriate endpoint on success
 * @return 0 on success, -1 on error
 */
int get_shortest_queue_endpoint(struct routing_table *table,
                                uint32_t key, struct msu_endpoint *endpoint);

/**
 * Gets endpoint witih given index in route set
 * @param table the route to get the endpoint from
 * @param index Index of the endpoint to retrieve
 * @param endpoint Points to appropriate endpoint on success
 * @return 0 on success, -1 on error
 */
int get_endpoint_by_index(struct routing_table *table, int index,
                          struct msu_endpoint *endpoint);

/**
 * Gets endpoint with the given MSU ID in the provided route
 * @param table Route to get the endpoint from
 * @param msu_id MSU ID to search for in the route set
 * @param endpoint Points to appropriate endpoint on success
 * @return 0 on success, -1 on error
 */
int get_endpoint_by_id(struct routing_table *table, int msu_id,
                       struct msu_endpoint *endpoint);

/**
 * Gets all of the endpoints in the provided routing table with the right runtime id.
 * @param table The table to search for endpoints
 * @param runtime_id The ID of the runtime on which endpoints must reside
 * @param endpoints An array of endpoints to be filled.
 * @param n_endpoints The amount of endpoints allocated in `endpoints`
 * @returns The number of endpoints stored in `endpoints`, or -1 if n_endpoints is too low
 */
int get_endpoints_by_runtime_id(struct routing_table *table, int runtime_id,
                                struct msu_endpoint *endpoints, int n_endpoints);

/**
 * Gets the route from the provided array of routes which has the correct type ID
 * @param routes Pointer to array of routes to search
 * @param n_routes Number of routes in route array
 * @param type_id Type ID to search for
 * @return pointer to the routing table  with correct type ID on success, else NULL
 */
struct routing_table *get_type_from_route_set(struct route_set *routes, int type_id);

int get_n_endpoints(struct routing_table *table);

/**
 * Adds an endpoint to the route with the given ID
 * @param route_id ID of the route to which the endpoint is added
 * @param endpoint the endpoint to be added
 * @param key Key to associate with endpoint
 * @return 0 on success, -1 on error
 */
int add_route_endpoint(int route_id, struct msu_endpoint endpoint, uint32_t key);

/**
 * Removes destination from route with given ID
 * @param route_id ID of the route from which the endpoint is removed
 * @param msu_id ID of the msu to remove
 * @return 0 on success, -1 on error
 */
int remove_route_endpoint(int route_id, int msu_id);

/**
 * Modifies key associated with MSU in route
 * @param route_id The ID of the route to modify
 * @param msu_id ID of MSU to modify
 * @param new_key New key to associate with MSU in route
 * @return 0 on success, -1 on error
 */
int modify_route_endpoint(int route_id, int msu_id, uint32_t new_key);

/**
 * Adds a route to a set of routes
 * @param route_set Set to add the route to
 * @param route_id ID of the route to add
 * @return 0 on success, -1 on error
 */
int add_route_to_set(struct route_set *set, int route_id);

/**
 * Removes a route from a set of routes
 * @param route_set Set to remove the route from
 * @param route_id ID of the route to remove
 * @returns 0 on success, -1 on error
 */
int rm_route_from_set(struct route_set *set, int route_id);

/** Initializes an endpoint structure to point to the relevant msu. */
int init_msu_endpoint(int msu_id, int runtime_id, struct msu_endpoint *endpoint);
#endif
