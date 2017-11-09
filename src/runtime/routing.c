/**
 * @file routing.c
 *
 * Functions for routing MSU messages between MSUs
 */

#include "dfg.h"
#include "routing.h"
#include "logging.h"
#include "runtime_dfg.h"
#include "local_msu.h"
#include "unused_def.h"
#include <stdlib.h>
#include <limits.h>
#include <pthread.h>

/** The maximum number of destinations a route can have */
#define MAX_DESTINATIONS 128

/** The maximum ID that may be assigned to a route */
#define MAX_ROUTE_ID 10000

/**
 * The core of the routing system, the routing table lists a route's destinations.
 * The routing_table is kept private so the rwlock can be enfoced.
 * All destinations in a routing table must have the same type_id.
 */
struct routing_table{
    int id;
    int type_id; /**< The type-id associated with the destinations in this table */
    pthread_rwlock_t rwlock; /**< Protects access to the destinations so they cannot be changed
                                  while they are being read */
    int n_endpoints;      /**< The number of destinations this route contains */
    uint32_t keys[MAX_DESTINATIONS]; /**< The keys associated with each of the destinations */
    struct msu_endpoint endpoints[MAX_DESTINATIONS]; /**< The destinations themselves */
};

#ifdef LOG_ROUTING_CHANGES
static void print_routing_table(struct routing_table *table) {
    char output[1024];
    int offset = sprintf(output,"\n---------- ROUTE TYPE: %d ----------\n", table->type_id);
    for (int i=0; i<table->n_endpoints; i++) {
        struct msu_endpoint *destination = &table->endpoints[i];
        offset += sprintf(output+offset, "- %4d: %3d\n", destination->id, (int)table->keys[i]);
    }
    log(LOG_ROUTING_CHANGES, "%s", output);
}
#else
#define print_routing_table(t)
#endif

/**
 * All of the created routes in this runtime
 */
static struct routing_table *all_routes[MAX_ROUTE_ID];

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
 * Searches through the entries in the routing table, returning the index of the first key
 * which surpasses the provided value
 * @param table the table to search
 * @param value the value to use in the search
 * @return the index of the destination with the appropriate value
 */
static int find_value_index(struct routing_table *table, uint32_t value) {
    //TODO: Binary search?
    int i = -1;
    if (table->n_endpoints == 0)
        return -1;
    value = value % table->keys[table->n_endpoints-1];
    for (i=0; i<table->n_endpoints; i++) {
        if (table->keys[i] > value)
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
    for (int i=0; i<table->n_endpoints; i++) {
        if (table->endpoints[i].id == msu_id)
            return i;
    }
    return -1;
}

/**
 * Removes a destination from the routing table
 * @param table The table from which to remove the entry
 * @param msu_id The ID of the msu to remove from the table
 * @return 0 on success, -1 on error
 */
static int rm_routing_table_entry(struct routing_table *table, int msu_id) {
    write_lock(table);
    int index = find_id_index(table, msu_id);
    if (index == -1) {
        log_error("MSU %d does not exist in route %d", msu_id, table->id);
        unlock(table);
        return -1;
    }

    // Shift destinations after removed index back by one
    for (int i=index; i<table->n_endpoints-1; ++i) {
        table->keys[i] = table->keys[i+1];
        table->endpoints[i] = table->endpoints[i+1];
    }
    table->n_endpoints--;
    unlock(table);
    log(LOG_ROUTING_CHANGES, "Removed destination %d from table %d (type %d)",
               msu_id, table->id, table->type_id);
    print_routing_table(table);
    return 0;
}

/**
 * Adds a (copy of the) destination to a routing table.
 * Note: Kept non-static but excluded from header so it can be used for testing
 * @param table The table to which the destination is to be added
 * @param destination The endpoint to add (a copy is made)
 * @param key The key to be associated with this destination
 * @return 0 on success, -1 on error
 */
int add_routing_table_entry(struct routing_table *table,
                            struct msu_endpoint *dest, uint32_t key) {
    write_lock(table);

    int i;
    if (table->n_endpoints >= MAX_DESTINATIONS) {
        log_error("Too many endpoints to add to route. Max: %d", MAX_DESTINATIONS);
        unlock(table);
        return -1;
    }

    // Shift all endpoints greater than the provided endpoint up by one
    for (i = table->n_endpoints; i > 0 && key < table->keys[i-1]; --i) {
        table->keys[i] = table->keys[i-1];
        table->endpoints[i] = table->endpoints[i-1];
    }
    table->keys[i] = key;
    table->endpoints[i] = *dest;
    table->endpoints[i].route_id = table->id;
    table->n_endpoints++;
    unlock(table);

    log(LOG_ROUTING_CHANGES, "Added destination %d to table %d (type: %d)",
               dest->id, table->id, table->type_id);
    print_routing_table(table);
    return 0;
}

/**
 * Modifies the key associated with an existing destination in a routing table
 * @param table The routing table to modify
 * @param msu_id The ID of the destination to modify
 * @param new_key The new key that should be associated with this destination
 * @return 0 on success, -1 on error
 */
static int alter_routing_table_entry(struct routing_table *table,
                                     int msu_id, uint32_t new_key) {
    write_lock(table);
    int index = find_id_index(table, msu_id);
    if (index == -1) {
        log_error("MSU %d does not exist in route %d", msu_id, table->id);
        unlock(table);
        return -1;
    }
    // TODO: Next two steps could be done in one operation

    int UNUSED orig_key = table->keys[index];

    struct msu_endpoint endpoint = table->endpoints[index];
    // Shift indices after removed back
    for (int i=index; i<table->n_endpoints-1; i++) {
        table->keys[i] = table->keys[i+1];
        table->endpoints[i] = table->endpoints[i+1];
    }

    // Shift indices after inserted forward
    int i;
    for (i = table->n_endpoints-1; i > 0 && new_key < table->keys[i-1]; i--) {
        table->keys[i] = table->keys[i-1];
        table->endpoints[i] = table->endpoints[i-1];
    }
    table->keys[i] = new_key;
    table->endpoints[i] = endpoint;
    unlock(table);
    log(LOG_ROUTING_CHANGES, "Changed key of msu %d in table %d from %d to %d",
               msu_id, table->id, orig_key, new_key);
    print_routing_table(table);
    return 0;
}

/**
 * Initializes and returns a new routing table.
 * Note: Not for external access! Use init_route()
 * Note: Kept non-static (but exclued from header) so it can be accessed during testing
 * @returns The new routing table
 */
struct routing_table *init_routing_table(int route_id, int type_id) {
    struct routing_table *table = calloc(1, sizeof(*table));
    if (table == NULL) {
        log_error("Error allocating routing table");
        return NULL;
    }
    table->id = route_id;
    table->type_id = type_id;
    pthread_rwlock_init(&table->rwlock, NULL);

    return table;
}

/**
 * Gets the routing table associated with the route_id
 * @returns A pointer to the routing table if it exists, or NULL
 */
static struct routing_table *get_routing_table(int route_id) {
    if (route_id > MAX_ROUTE_ID) {
        log_error("Cannot get route with ID > %d", MAX_ROUTE_ID);
        return NULL;
    }
    return all_routes[route_id];
}


int init_route(int route_id, int type_id) {
    if (route_id > MAX_ROUTE_ID) {
        log_error("Cannot initialize route with ID > %d", MAX_ROUTE_ID);
        return -1;
    }
    struct routing_table *table = get_routing_table(route_id);
    if (table != NULL) {
        log_error("Route with id %d already exists on runtime", route_id);
        return -1;
    }
    table = init_routing_table(route_id, type_id);
    if (table == NULL) {
        log_error("Error initializing routing table %d", route_id);
        return -1;
    }
    all_routes[route_id] = table;
    log(LOG_ROUTING_CHANGES, "Initialized route %d (type %d)", route_id, type_id);
    return 0;
}

// TODO: Redefine, move  MAX_MSU_Q_SIZE!!
#define MAX_MSU_Q_SIZE INT_MAX

int get_shortest_queue_endpoint(struct routing_table *table,
                                uint32_t key, struct msu_endpoint *endpoint) {
    read_lock(table);

    unsigned int shortest_queue = MAX_MSU_Q_SIZE;
    int n_shortest = 0;
    struct msu_endpoint *endpoints[table->n_endpoints];

    for (int i=0; i<table->n_endpoints; i++) {
        if (table->endpoints[i].locality == MSU_IS_REMOTE) {
            continue;
        }
        int length = table->endpoints[i].queue->num_msgs;
        if (length < shortest_queue) {
            endpoints[0] = &table->endpoints[i];
            shortest_queue = length;
            n_shortest = 1;
        } else if (length == shortest_queue) {
            endpoints[n_shortest] = &table->endpoints[i];
            n_shortest++;
        }
    }

    if (n_shortest == 0) {
        unlock(table);
        return -1;
    }

    *endpoint = *endpoints[key % (int)n_shortest];
    unlock(table);
    return 0;
}

int get_endpoint_by_index(struct routing_table *table, int index, 
                          struct msu_endpoint *endpoint) {
    int rtn = -1;
    read_lock(table);
    if (table->n_endpoints > index) {
        *endpoint = table->endpoints[index];
        rtn = 0;
    }
    unlock(table);
    return rtn;
}

int get_endpoint_by_id(struct routing_table *table, int msu_id, 
                       struct msu_endpoint *endpoint) {
    int rtn = -1;
    read_lock(table);
    for (int i=0; i < table->n_endpoints; i++) {
        if (table->endpoints[i].id == msu_id) {
            *endpoint = table->endpoints[i];
            rtn = 0;
            break;
        }
    }
    unlock(table);
    return rtn;
}

int get_endpoints_by_runtime_id(struct routing_table *table, int runtime_id,
                                struct msu_endpoint *endpoints, int n_endpoints) {
    int found_endpoints = 0;
    read_lock(table);
    for (int i=0; i < table->n_endpoints; i++) {
        if (table->endpoints[i].runtime_id == runtime_id) {
            if (n_endpoints <= found_endpoints) {
                log_error("Not enough endpoints passed in to hold results");
                unlock(table);
                return -1;
            }
            endpoints[found_endpoints] = table->endpoints[i];
            found_endpoints++;
        }
    }
    unlock(table);
    return found_endpoints;
}

int get_n_endpoints(struct routing_table *table) {
    read_lock(table);
    int n_endpoints = table->n_endpoints;
    unlock(table);
    return n_endpoints;
}

int get_route_endpoint(struct routing_table *table, uint32_t key, struct msu_endpoint *endpoint) {
    read_lock(table);
    int index = find_value_index(table, key);
    if (index < 0) {
        log_error("Could not find index for key %d", key);
        unlock(table);
        return -1;
    }
    *endpoint = table->endpoints[index];
    log(LOG_ROUTING_DECISIONS, "Endpoint for key %u is %d", key, endpoint->id);
    unlock(table);
    return 0;
}

struct routing_table *get_type_from_route_set(struct route_set *set, int type_id) {
    for (int i=0; i<set->n_routes; i++) {
        if (set->routes[i]->type_id == type_id) {
            return set->routes[i];
        }
    }
    log_error("No route available of type %d", type_id);
    return NULL;
}

int add_route_endpoint(int route_id, struct msu_endpoint endpoint, uint32_t key) {
    log(LOG_ROUTING_CHANGES, "Adding endpoint %d to route %d", endpoint.id, route_id);
    struct routing_table *table = get_routing_table(route_id);
    if (table == NULL) {
        log_error("Route %d does not exist", route_id);
        return -1;
    }
    return add_routing_table_entry(table, &endpoint, key);
}

int remove_route_endpoint(int route_id, int msu_id) {
    struct routing_table *table = get_routing_table(route_id);
    if (table == NULL) {
        log_error("Route %d does not exist", route_id);
        return -1;
    }
    int rtn = rm_routing_table_entry(table, msu_id);
    if (rtn == -1) {
        log_error("Error removing msu %d from route %d", msu_id, route_id);
        return -1;
    }
    log(LOG_ROUTING_CHANGES, "Removed destination %d from route %d", msu_id, route_id);
    return 0;
}

int modify_route_endpoint(int route_id, int msu_id, uint32_t new_key) {
    struct routing_table *table = get_routing_table(route_id);
    if (table == NULL) {
        log_error("Route %d does not exist", route_id);
        return -1;
    }
    int rtn = alter_routing_table_entry(table, msu_id, new_key);
    if (rtn < 0) {
        log_error("Error altering routing for msu %d on route %d", msu_id, route_id);
        return -1;
    }
    log(LOG_ROUTING_CHANGES, "Altered key %d for msu %d in route %d",
               new_key, msu_id, route_id);
    return 0;
}

int add_route_to_set(struct route_set *set, int route_id) {
    struct routing_table *table = get_routing_table(route_id);
    if (table == NULL) {
        log_error("Route %d does not exist", route_id);
        return -1;
    }
    set->routes = realloc(set->routes, sizeof(*set->routes) * (set->n_routes + 1));
    if (set->routes == NULL) {
        log_error("Error reallocating routes");
        return -1;
    }
    set->routes[set->n_routes] = table;
    set->n_routes++;
    return 0;
}
int rm_route_from_set(struct route_set *set, int route_id) {
    int i;
    for (i=0; i<set->n_routes; i++) {
        if (set->routes[i]->id == route_id) {
            break;
        }
    }
    if (i == set->n_routes) {
        log_error("route %d does not exist in set", route_id);
        return -1;
    }
    for (; i<set->n_routes - 1; i++) {
        set->routes[i] = set->routes[i+1];
    }
    set->n_routes--;
    return 0;
}

int init_msu_endpoint(int msu_id, int runtime_id, struct msu_endpoint *endpoint) {
    endpoint->id = msu_id;
    int local_id = local_runtime_id();
    if (local_id < 0) {
        log_error("Cannot get local runtime ID");
        return -1;
    }
    if (runtime_id == local_id) {
        endpoint->locality = MSU_IS_LOCAL;
        struct local_msu *msu = get_local_msu(msu_id);
        if (msu == NULL) {
            log_error("Cannot find local MSU with id %d for initializing endpoint",
                      msu_id);
            return -1;
        }
        endpoint->queue = &msu->queue;
        endpoint->runtime_id = runtime_id;
    } else {
        endpoint->locality = MSU_IS_REMOTE;
        endpoint->runtime_id = runtime_id;
    }
    return 0;
}


