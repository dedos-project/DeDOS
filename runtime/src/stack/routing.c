#include "routing.h"
#include "ip_utils.h"
#include "msu_tracker.h"

/** The number of destinations a route can have before it is reallocated */
#define DEFAULT_MAX_DESTINATIONS 16

/** The core of the routing system, the routing_table contains a list a route's destinations.
 * The routing_table is kept private so the rwlock can be enforced for all access.
 * All destinations in a routing table must have the same type_id.
 */
struct routing_table{
    int type_id; /**< The type-id associated with the destinations in this table */
    int n_refs;

    pthread_rwlock_t rwlock; /**< Protects access to the destinations so they cannot be changed
                                  while they are being read */
    int n_destinations;      /**< The number of destinations this route contains */
    int max_destinations;    /**< The number of destinations that this route has allocated */
    uint32_t *ranges;        /**< The keys associated with each of the destinations */
    struct msu_endpoint *destinations; /**< The destinations themselves */
};

/**
 * This file keeps track of all of the created routes
 */
static struct route_set *all_routes = NULL;

/**
 * Simple wrapper to lock a routing table for reading
 * @param table the routing table to be locked for reading
 * @return 0 on success, -1 on error
 */
static int read_lock(struct routing_table *table) {
    return pthread_rwlock_rdlock(&table->rwlock);
}

/**
 * Simple wrapper to lock a routing table for writing
 * @param table the routing table to be locked for writing
 * @return 0 on success, -1 on error
 */
static int write_lock(struct routing_table *table) {
    return pthread_rwlock_wrlock(&table->rwlock);
}

/**
 * Simple wrapper to unlock a locked routing table
 * @param table the routing table to be unlocked
 * @return 0 on success, -1 on error
 */
static int unlock(struct routing_table *table) {
    return pthread_rwlock_unlock(&table->rwlock);
}

/**
 * Reallocates the keys and destinations in a table when the number of entries
 * has surpassed the number of allocated spots.
 * @param table the table containing the ranges and destinations to reallocate
 * @param max_entries the number of entries that will be allocated in the table
 * @return 0 on success, -1 on error
 */
static int realloc_table_entries(struct routing_table *table, int max_entries) {
    table->ranges = realloc(table->ranges, sizeof(uint32_t) * max_entries);
    if (table->ranges == NULL) {
        log_error("Error allocating table ranges");
        return -1;
    }

    table->destinations = realloc(table->destinations, sizeof(struct msu_endpoint) * max_entries);
    if (table->destinations == NULL) {
        log_error("error allocating table destinations");
        free(table->ranges);
        return -1;
    }

    table->max_destinations = max_entries;
    return 0;
}

/**
 * Searches through the entries in the routing table, returning the index of the first key
 * which surpasses the provided value
 * @param table the table to search
 * @param value the value to use in the search
 * @return the index of the destination with the appropriate value
 */
static int find_value_index(struct routing_table *table, uint32_t value) {
    //TODO: Binary search?
    int i = -1;
    if (table->n_destinations == 0)
        return -1;
    value = value % table->ranges[table->n_destinations-1];
    for (i=0; i<table->n_destinations; i++) {
        if (table->ranges[i] > value)
            return i;
    }
    return i;
}

/**
 * Finds the index of the entry in the routing table with the destination that has the provided
 * msu id
 * @param table the table to search
 * @param msu_id the ID to search for in the table
 * @return the index of the destination with the appropriate msu id
 */
static int find_id_index(struct routing_table *table, int msu_id) {
    for (int i=0; i<table->n_destinations; i++) {
        if (table->destinations[i].id == msu_id)
            return i;
    }
    return -1;
}

/**
 * Removes a destination from a routing table
 * @param table the table to remove the entry from
 * @param msu_id the ID of the msu to remove from the routing table
 * @return pointer to the msu_endpoint removed from the routing table, or NULL if not found
 */
static struct msu_endpoint *rm_routing_table_entry(struct routing_table *table, int msu_id) {
    write_lock(table);;
    int index = find_id_index(table, msu_id);
    if (index == -1) {
        log_error("MSU %d does not exist in specified route", msu_id);
        unlock(table);
        return NULL;
    }
    struct msu_endpoint *to_remove = &table->destinations[index];
    for (int i=index; i<table->n_destinations-1; i++) {
        table->ranges[i] = table->ranges[i+1];
        table->destinations[i] = table->destinations[i+1];
    }
    table->n_destinations--;
    unlock(table);
    return to_remove;
}

/**
 * Adds a destination to a routing table.
 * @param table the routing table to which the destination should be added
 * @param destination the endpoint to add to the table
 * @param range_end the key that is associated with this destination
 * @return 0 on success, -1 on error
 */
static int add_routing_table_entry(struct routing_table *table,
                                   struct msu_endpoint *destination, uint32_t range_end) {
    write_lock(table);
    if (table->type_id == 0) {
        table->type_id = destination->type_id;
    }

    if (range_end <= 0) {
        log_error("Cannot add routing table entry with non-positive key %d", range_end);
        return -1;
    }


    if (destination->type_id != table->type_id) {
        log_error("Cannot add mixed-types to a single route. "
                  "Attempting to add %d to route of type %d. ",
                  destination->type_id, table->type_id);
        unlock(table);
        return -1;
    }

    if (table->n_destinations == table->max_destinations) {
        int rtn = realloc_table_entries(table,
                table->max_destinations + DEFAULT_MAX_DESTINATIONS);
        if (rtn < 0) {
            log_error("Could not add new route endpoint");
            unlock(table);
            return -1;
        }
    }

    int i;
    for (i=table->n_destinations; i > 0 && range_end < table->ranges[i-1]; i--) {
        table->ranges[i] = table->ranges[i-1];
        table->destinations[i] = table->destinations[i-1];
    }
    table->ranges[i] = range_end;
    table->destinations[i] = *destination;
    table->n_destinations++;
    unlock(table);
    log_info("Added destination %d to table of type %d", destination->id, table->type_id);
    return 0;
}

/**
 * Modifies the key associated with an existing destination in a routing table
 * @param table The routing table to modify
 * @param msu_id the ID of the destination to modify
 * @param new_range_end the new key that should be associated with this destination
 * @return 0 on success, -1 on error
 */
static int alter_routing_table_entry(struct routing_table *table,
                                     int msu_id, uint32_t new_range_end) {
    struct msu_endpoint *endpoint = rm_routing_table_entry(table, msu_id);
    if (endpoint == NULL) {
        return -1;
    }
    return add_routing_table_entry(table, endpoint, new_range_end);
}

/**
 * Initializes and returns a new routing table.
 * NOTE: not for external access! Use init_route()
 * @returns the new routing table
 */
static struct routing_table *init_routing_table() {
    struct routing_table *table = malloc(sizeof(*table));
    if (table == NULL) {
        log_error("Error allocating routing table");
        return NULL;
    }
    table->type_id = 0;
    table->n_refs = 0;
    table->n_destinations = 0;
    table->max_destinations = 0;
    table->ranges = NULL;
    table->destinations = NULL;
    pthread_rwlock_init(&table->rwlock, NULL);

    int rtn = realloc_table_entries(table, DEFAULT_MAX_DESTINATIONS);
    if (rtn < 0) {
        log_error("Error allocating table entry");
        free(table);
        return NULL;
    }

    return table;
}

/**
 * Gets the routing table associated with a given route_id, creating it if it does not exist
 * @returns a pointer to the routing table
 */
static struct routing_table *get_routing_table(int route_id) {
    struct route_set *route = NULL;
    HASH_FIND_INT(all_routes, &route_id, route);
    if (route == NULL) {
        log_debug("Creating new routing table (id = %d)", route_id);
        route = malloc(sizeof(*route));
        if (route == NULL) {
            log_error("Error allocating new routing table");
            return NULL;
        }
        route->id = route_id;
        route->table = init_routing_table();
        if (route->table == NULL) {
            log_error("Error creating routing table");
            free(route);
            return NULL;
        }
        ++route->table->n_refs;
        HASH_ADD_INT(all_routes, id, route);
    }
    return route->table;
}

/**
 * Initializes a new route with the given route_id and type_id.
 * If the route already exists, does nothing if the type_id matches the existing type_id
 * @return 0 on success, -1 if type id does not match
 */
int init_route(int route_id, int type_id) {
    struct routing_table *table = get_routing_table(route_id);
    if (table->type_id == 0) {
        table->type_id = type_id;
        log_debug("Set type of route %d to %d", route_id, type_id);
    }
    if (table->type_id != 0 && table->type_id != type_id) {
        log_warn("Type ID of route %d is %d, not %d", route_id, type_id, table->type_id);
        return -1;
    }
    return 0;
}

/**
 * Gets the local endpoint from a route_set with the shortest queue length
 * @param routes the route_set to search for the shortest queue length
 * @key Used as a tiebreaker in the case of multiple MSUs with the same queue length
 * @return pointer to the endpoint with the shortest queue
 */
struct msu_endpoint *get_shortest_queue_endpoint(struct route_set *routes, uint32_t key) {
    struct routing_table *table = routes->table;
    read_lock(table);

    unsigned int shortest_queue = MAX_MSU_Q_SIZE;
    int n_shortest = 0;
    struct msu_endpoint *best_endpoints[table->n_destinations];

    for (int i=0; i<table->n_destinations; i++) {
        if ( table->destinations[i].locality == MSU_IS_REMOTE ) {
            continue;
        }
        int length = table->destinations[i].msu_queue->num_msgs;
        if (length < shortest_queue ) {
            best_endpoints[0] = &table->destinations[i];
            shortest_queue = length;
            n_shortest = 1;
        } else if (length == shortest_queue ) {
            best_endpoints[n_shortest] = &table->destinations[i];
            n_shortest++;
        }
    }

    if ( n_shortest == 0){
        unlock(table);
        return NULL;
    }

    struct msu_endpoint *best_endpoint = best_endpoints[key % (int)n_shortest];

    unlock(table);
    return best_endpoint;
}

/**
 * Gets the endpoint with the given index in the provided route set
 * @param routes the route set to get the endpoint from
 * @param index the index of the endpoint to retrieve
 * @return pointer to the endpoint at the given index, or NULL if index > len(routes)
 */
struct msu_endpoint *get_endpoint_by_index(struct route_set *routes, int index) {
    struct routing_table *table = routes->table;
    read_lock(table);
    struct msu_endpoint *msu = NULL;
    if (table->n_destinations > index) {
        msu = &table->destinations[index];
    }
    unlock(table);
    return msu;
}

/**
 * Gets the endpoint with the given MSU ID in the provided route set
 * @param routes the route set to get the endpoint from
 * @param msu_id the MSU ID to search for in the route set
 * @return pointer to the endpoint with the given ID if it exists, otherwise NULL
 */
struct msu_endpoint *get_endpoint_by_id(struct route_set *routes, int msu_id) {
    struct routing_table *table = routes->table;
    read_lock(table);
    struct msu_endpoint *msu = NULL;
    for (int i=0; i<table->n_destinations; i++) {
        if (table->destinations[i].id == msu_id) {
            msu = &table->destinations[i];
            break;
        }
    }
    unlock(table);
    return msu;
}

/**
 * Helper function to get the queue from the MSU with the given ID and type ID.
 * @param msu_id the ID of the MSU from which to get the queue pointer
 * @param msu_type_id the type ID of the MSU (for error checking)
 * @return pointer to the msu_queue of the msu with the given ID, or NULL if DNE
 */
struct generic_msu_queue *get_msu_queue(int msu_id, int msu_type_id) {
    struct msu_placement_tracker *tracker= msu_tracker_find(msu_id);
    if (!tracker) {
        log_error("Couldn't find msu tracker for MSU %d", msu_id);
        return NULL;
    }

    struct generic_msu *msu = dedos_msu_pool_find(tracker->dedos_thread->msu_pool,
                                                  msu_id);
    if (!msu) {
        log_error("Failed to get ptr to MSU %d from msu pool", msu_id);
        return NULL;
    }

    if (msu->type->type_id != msu_type_id) {
        log_error("Found msu with matching ID %d but mismatching types %d!=%d",
                  msu_id, msu_type_id, msu->type->type_id);
        return NULL;
    }
    return &msu->q_in;
}

/**
 * Gets the endpoint associated with the given key in the provided route set
 * @param routes the route set to search for the key
 * @param key the key to search the route set with
 * @return pointer to the MSU endpoint, NULL on error
 */
struct msu_endpoint *get_route_endpoint(struct route_set *routes, uint32_t key) {
    struct routing_table *table = routes->table;
    read_lock(table);
    int index = find_value_index(table, key);
    if (index < 0) {
        log_error("Could not find index for key %d", key);
        unlock(table);
        return NULL;
    }
    struct msu_endpoint *endpoint = &table->destinations[index];
    log_debug("Endpoint for key %u is %d", key, endpoint->id);
    unlock(table);
    return endpoint;
}

/**
 * Initialize a reference to a route with the given ID, which can be added to an MSU's route set
 * @param route_id the ID of the routing table to reference
 * @return pointer to the newly allocated route set if the route exists, NULL otherwise
 */
struct route_set *init_route_set(int route_id) {
    struct routing_table *table = get_routing_table(route_id);
    if (table == NULL) {
        log_error("Error getting routing table");
        return NULL;
    }
    struct route_set *route = malloc(sizeof(*route));
    if (route == NULL) {
        log_error("Error allocating new route set");
        return NULL;
    }
    route->table = table;
    write_lock(table);
    ++(route->table->n_refs);
    unlock(table);
    return route;
}

/**
 * Frees a route set and decriments the number of references to the route
 * @param route the route set to destroy
 */
void destroy_route_set(struct route_set *route) {
    struct routing_table *table = route->table;
    write_lock(table);
    --(table->n_refs);
    free(route);
    unlock(table);
}

/**
 * Adds a route the the provided route set with the given route_id
 * @param routes double-pointer to the route_set to which the route is to be added
 * @param route_id the ID of the route to be added to the route set
 * @return 0 on success, -1 on error
 */
int add_route_to_set(struct route_set **routes, int route_id) {
    // Get the existing route  so we can get the type id
    struct route_set *route = init_route_set(route_id);
    if (route == NULL) {
        log_error("Cannot add route %d -- route does not exist", route_id);
        return -1;
    }

    // Modify the route ID to match the type ID
    route->id = route->table->type_id;

    // See if a route of that type already exists in the set
    struct route_set *existing_route = NULL;
    HASH_FIND_INT(*routes, &route->id, existing_route);
    if (existing_route == NULL) {
        HASH_ADD_INT(*routes, id, route);
        log_debug("Added route %d (type %d) to set", route_id, route->id);
    } else {
        log_error("Could not add route %d -- route with id %d already exists in set",
                  route_id, route->id);
        destroy_route_set(route);
        return -1;
    }
    return 0;
}

/**
 * Deletes a route from the provided set
 * @param routes double-pointer to the route_set from which the route is to be removed
 * @param route_id the ID of the route to be removed
 * @return 0 on success, -1 on error
 */
int del_route_from_set(struct route_set **routes, int route_id) {
    // First find it in the global table so we can get type id
    struct route_set *route = NULL;
    HASH_FIND_INT(all_routes, &route_id, route);
    if (route == NULL) {
        log_debug("Could not delete route %d -- route does not exist", route_id);
        return -1;
    }
    int type_id = route->table->type_id;

    // Now get it from the passed in route set
    HASH_FIND_INT(*routes, &type_id, route);
    if (route == NULL) {
        log_debug("Could not delete route %d -- route not in route set", route_id);
        return -1;
    }

    HASH_DEL(*routes, route);
    destroy_route_set(route);
    log_debug("Removed route %d from set", route_id);
    return 0;
}

/**
 * Gets the route_set from the provided array of sets with the given type_id
 * @param routes double-pointer to the route-set from which to retrieve the route
 * @param type_id the type of the route to retrieve
 * @return pointer to the route_set with the correct type_id on success, NULL on failure
 */
struct route_set *get_type_from_route_set(struct route_set **routes, int type_id) {
    struct route_set *type_set = NULL;
    HASH_FIND_INT(*routes, &type_id, type_set);
    if (type_set == NULL) {
        log_error("No routes available of type %d", type_id);
        for (struct route_set *route = *routes; route != NULL; route=route->hh.next)
            log_debug("Available: %d", route->id);
        return NULL;
    }
    return type_set;
}

/**
 * Adds an endpoint to the route with the given ID
 * @param route_id the ID of the route to which the endpoint is to be added
 * @param endpoint the endpoint to be added
 * @param range_end the key associated with the endpoint
 * @return 0 on success, -1 on error
 */
int add_route_endpoint(int route_id, struct msu_endpoint *endpoint, uint32_t range_end) {
    struct routing_table *table = get_routing_table(route_id);
    return add_routing_table_entry(table, endpoint, range_end);
}

/**
 * Removes a destination from a route with the given ID
 * @param route_id the ID of the route from which the endpoint is to be removed
 * @param msu_id the ID of the msu to remove from the route
 * @return 0 on success, -1 on error
 */
int remove_route_destination(int route_id, int msu_id) {
    struct routing_table *table = get_routing_table(route_id);
    struct msu_endpoint *endpoint = rm_routing_table_entry(table, msu_id);
    if (endpoint == NULL) {
        log_error("Error removing msu %d from route %d", msu_id, route_id);
        return -1;
    }
    free(endpoint);
    return 0;
}

/**
 * Removes a destination from all of the routes which contain it
 * @param msu_id the ID of the msu to remove from all routes
 */
void remove_destination_from_all_routes(int msu_id) {
    // TODO: This is slow. Should only care about routes we're interested in
    for (struct route_set *ref = all_routes; ref != NULL; ref=ref->hh.next) {

        struct msu_endpoint *endpoint = rm_routing_table_entry(ref->table, msu_id);
        if (endpoint != NULL) {
            free(endpoint);
        }
    }
}

/**
 * Modifies the key associated with the MSU in the given route
 * @param route_id the ID of the route to modify
 * @param msu_id the ID of the MSU in the route to modify
 * @param new_range_end the new key to associate with that MSU in that route
 * @return 0 on success, -1 on error
 */
int modify_route_key_range(int route_id, int msu_id, uint32_t new_range_end) {
    struct routing_table *table = get_routing_table(route_id);
    int rtn = alter_routing_table_entry(table, msu_id, new_range_end);
    if (rtn < 0) {
        log_error("Error altering routing for msu %d on route %d", msu_id, route_id);
        return -1;
    }
    return 0;
}
