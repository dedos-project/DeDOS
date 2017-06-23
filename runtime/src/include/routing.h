#pragma once
/** routing.h
 *
 * Defines routing table structure for each msu
 * and function declarations to modify the routing table
 *
 */

#include "control_protocol.h"
#include "generic_msu_queue.h"
#include "uthash.h"

/**
 * A structure to hold a local or remote MSU that is a route destination
 */
struct msu_endpoint{
    int id;         /**< The ID of the MSU located at this endpoint */
    int type_id;    /**< The type-ID of the MSU */
    enum locality locality; /**< Either MSU_IS_LOCAL or MSU_IS_REMOTE */
    uint32_t ipv4;  /**< If remote, ip of the remote runtime */
    struct generic_msu_queue *msu_queue; /**< If local, pointer to the MSU's queue */
};

/* 
 * Forward declaration so reads and writes have
 * to take place through the API (to adhere to mutex)
 */
struct routing_table;

/**
 * A structure to hold references to a routing table, 
 * which can be referenced by either id or type-id of the route
 */
struct route_set{
    int id;     /**< An identifier for the route (either ID or type-ID, depending on usage) */
    struct routing_table *table; /**< The routing table being referenced */
    UT_hash_handle hh; /**< A hash handle for quick lookups of tables */
};


/** Helper function to get the queue from the MSU with the given ID and type ID */
struct generic_msu_queue *get_msu_queue(int msu_id, int msu_type_id);
/** Gets the endpoint associated with the given key in the provided route set */
struct msu_endpoint *get_route_endpoint(struct route_set *routes, uint32_t key);
/** Gets the local endpoint from a route_set with the shortest queue length */
struct msu_endpoint *get_shortest_queue_endpoint(struct route_set *routes, uint32_t key);
/** Gets the endpoint with the given index in the provided route set */
struct msu_endpoint *get_endpoint_by_index(struct route_set *routes, int index);
/** Gets the endpoint with the given MSU ID in the provided route set */
struct msu_endpoint *get_endpoint_by_id(struct route_set *routes, int msu_id);

/** Initializes a new route with the given route_id and type_id */
int init_route(int route_id, int type_id);
/** Modifies the key associated with the MSU in the given route */
int modify_route_key_range(int route_id, int msu_id, uint32_t new_range_end);
/** Adds an endpoint to the route with the given ID */
int add_route_endpoint(int route_id, struct msu_endpoint *endpoint, uint32_t range_end);
/** Removes a destination from a route with the given ID */
int remove_route_destination(int route_id, int msu_id);
/** Removes a destination from all of the routes which contain it */
void remove_destination_from_all_routes(int msu_id);

/** Gets the route_set from the provided array of sets with the given type_id */
struct route_set *get_type_from_route_set(struct route_set **routes, int type_id);
/** Adds a route the the provided route set with the given route_id */
int add_route_to_set(struct route_set **routes, int route_id);
/** Deletes a route from the provided set */
int del_route_from_set(struct route_set **routes, int route_id);
void print_route_set(struct route_set *routes);
