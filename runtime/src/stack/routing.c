#include "routing.h"
#include "ip_utils.h"
#include "msu_tracker.h"

#define DEFAULT_MAX_DESTINATIONS 16

struct routing_table{
    int type_id;

    int n_refs;

    pthread_rwlock_t rwlock;

    int n_destinations;
    int max_destinations;
    uint32_t *ranges;
    struct msu_endpoint *destinations;
};

static int read_lock(struct routing_table *table){
    return pthread_rwlock_rdlock(&table->rwlock);
}

static int write_lock(struct routing_table *table){
    return pthread_rwlock_wrlock(&table->rwlock);
}

static int unlock(struct routing_table *table){
    return pthread_rwlock_unlock(&table->rwlock);
}


static struct route_set *all_routes = NULL;

static int realloc_table_entries(struct routing_table *table, int max_entries){
    table->ranges = realloc(table->ranges, sizeof(uint32_t) * max_entries);
    if (table->ranges == NULL){
        log_error("Error allocating table ranges");
        return -1;
    }

    table->destinations = realloc(table->destinations, sizeof(struct msu_endpoint) * max_entries);
    if (table->destinations == NULL){
        log_error("error allocating table destinations");
        free(table->ranges);
        return -1;
    }

    table->max_destinations = max_entries;
    return 0;
}

static int find_value_index(struct routing_table *table, uint32_t value){
    //TODO: Binary search?
    int i;
    if (table->n_destinations == 0)
        return -1;
    value = value % table->ranges[table->n_destinations-1];
    for (i=0; i<table->n_destinations; i++){
        if (table->ranges[i] > value)
            return i;
    }
    return i;
}

static int find_id_index(struct routing_table *table, int msu_id){
    for (int i=0; i<table->n_destinations; i++){
        if (table->destinations[i].id == msu_id)
            return i;
    }
    return -1;
}

static struct msu_endpoint *rm_routing_table_entry(struct routing_table *table, int msu_id){
    write_lock(table);;
    int index = find_id_index(table, msu_id);
    if (index == -1){
        log_error("MSU %d does not exist in specified route", msu_id);
        unlock(table);
        return NULL;
    }
    struct msu_endpoint *to_remove = &table->destinations[index];
    for (int i=index; i<table->n_destinations-1; i++){
        table->ranges[i] = table->ranges[i+1];
        table->destinations[i] = table->destinations[i+1];
    }
    table->n_destinations--;
    unlock(table);
    return to_remove;
}

static int add_routing_table_entry(struct routing_table *table,
                                   struct msu_endpoint *destination, uint32_t range_end){
    write_lock(table);
    if (table->type_id == 0){
        table->type_id = destination->type_id;
    }

    if (range_end <= 0){
        log_error("Cannot add routing table entry with non-positive key %d", range_end);
        return -1;
    }


    if (destination->type_id != table->type_id){
        log_error("Cannot add mixed-types to a single route. "
                  "Attempting to add %d to route of type %d. ",
                  destination->type_id, table->type_id);
        unlock(table);
        return -1;
    }

    if (table->n_destinations == table->max_destinations){
        int rtn = realloc_table_entries(table,
                table->max_destinations + DEFAULT_MAX_DESTINATIONS);
        if (rtn < 0){
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
    log_info("Added destinoation %d to table of type %d", destination->id, table->type_id);
    return 0;
}

static int alter_routing_table_entry(struct routing_table *table,
                                     int msu_id, uint32_t new_range_end){
    struct msu_endpoint *endpoint = rm_routing_table_entry(table, msu_id);
    if (endpoint == NULL){
        return -1;
    }
    return add_routing_table_entry(table, endpoint, new_range_end);
}

static struct routing_table *init_routing_table(){
    struct routing_table *table = malloc(sizeof(*table));
    if (table == NULL){
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
    if (rtn < 0){
        log_error("Error allocating table entry");
        free(table);
        return NULL;
    }

    return table;
}

static struct routing_table *get_routing_table(int route_id){
    struct route_set *route = NULL;
    HASH_FIND_INT(all_routes, &route_id, route);
    if (route == NULL){
        log_debug("Creating new routing table (id = %d)", route_id);
        route = malloc(sizeof(*route));
        if (route == NULL){
            log_error("Error allocating new routing table");
            return NULL;
        }
        route->id = route_id;
        route->table = init_routing_table();
        if (route->table == NULL){
            log_error("Error creating routing table");
            free(route);
            return NULL;
        }
        ++route->table->n_refs;
        HASH_ADD_INT(all_routes, id, route);
    }
    return route->table;
}

int init_route(int route_id, int type_id){
    struct routing_table *table = get_routing_table(route_id);
    if (table->type_id == 0){
        table->type_id = type_id;
        log_debug("Set type of route %d to %d", route_id, type_id);
    }
    if (table->type_id != 0 && table->type_id != type_id){
        log_warn("Type ID of route %d is %d, not %d", route_id, type_id, table->type_id);
        return -1;
    }
    return 0;
}

struct msu_endpoint *get_shortest_queue_endpoint(struct route_set *routes){
    struct routing_table *table = routes->table;
    read_lock(table);
    struct msu_endpoint *msu = NULL;

    unsigned int shortest_queue = MAX_MSU_Q_SIZE;
    struct msu_endpoint *best_endpoint = NULL;

    for (int i=0; i<table->n_destinations; i++){
        if ( table->destinations[i].locality == MSU_IS_REMOTE ) {
            continue;
        }
        int length = table->destinations[i].msu_queue->num_msgs;
        if (length < shortest_queue ) {
            shortest_queue = length;
            best_endpoint = &table->destinations[i];
        }
    }

    unlock(table);
    return best_endpoint;
}

struct msu_endpoint *get_endpoint_by_index(struct route_set *routes, int index){
    struct routing_table *table = routes->table;
    read_lock(table);
    struct msu_endpoint *msu = NULL;
    if (table->n_destinations > index) {
        msu = &table->destinations[index];
    }
    unlock(table);
    return msu;
}

struct msu_endpoint *get_endpoint_by_id(struct route_set *routes, int msu_id){
    struct routing_table *table = routes->table;
    read_lock(table);
    struct msu_endpoint *msu = NULL;
    for (int i=0; i<table->n_destinations; i++){
        if (table->destinations[i].id == msu_id){
            msu = &table->destinations[i];
            break;
        }
    }
    unlock(table);
    return msu;
}

struct generic_msu_queue *get_msu_queue(int msu_id, int msu_type_id){
    struct msu_placement_tracker *tracker= msu_tracker_find(msu_id);
    if (!tracker){
        log_error("Couldn't find msu tracker for MSU %d", msu_id);
        return NULL;
    }

    struct generic_msu *msu = dedos_msu_pool_find(tracker->dedos_thread->msu_pool,
                                                  msu_id);
    if (!msu){
        log_error("Failed to get ptr to MSU %d from msu pool", msu_id);
        return NULL;
    }

    if (msu->type->type_id != msu_type_id){
        log_error("Found msu with matching ID %d but mismatching types %d!=%d",
                  msu_id, msu_type_id, msu->type->type_id);
        return NULL;
    }
    return &msu->q_in;
}


struct msu_endpoint *get_route_endpoint(struct route_set *routes, uint32_t key){
    struct routing_table *table = routes->table;
    read_lock(table);
    int index = find_value_index(table, key);
    if (index < 0){
        log_error("Could not find index for key %d", key);
        unlock(table);
        return NULL;
    }
    struct msu_endpoint *endpoint = &table->destinations[index];
    log_debug("Endpoint for key %u is %d", key, endpoint->id);
    unlock(table);
    return endpoint;
}

struct route_set *init_route_set(int route_id){
    struct routing_table *table = get_routing_table(route_id);
    if (table == NULL){
        log_error("Error getting routing table");
        return NULL;
    }
    struct route_set *route = malloc(sizeof(*route));
    if (route == NULL){
        log_error("Error allocating new route set");
        return NULL;
    }
    route->table = table;
    write_lock(table);
    ++(route->table->n_refs);
    unlock(table);
    return route;
}

void destroy_route_set(struct route_set *route){
    struct routing_table *table = route->table;
    write_lock(table);
    --(table->n_refs);
    free(route);
    unlock(table);
}

int add_route_to_set(struct route_set **routes, int route_id){
    // Get the existing route  so we can get the type id
    struct route_set *route = init_route_set(route_id);
    if (route == NULL){
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

int del_route_from_set(struct route_set **routes, int route_id){
    // First find it in the global table so we can get type id
    struct route_set *route = NULL;
    HASH_FIND_INT(all_routes, &route_id, route);
    if (route == NULL){
        log_debug("Could not delete route %d -- route does not exist", route_id);
        return -1;
    }
    int type_id = route->table->type_id;

    // Now get it from the passed in route set
    HASH_FIND_INT(*routes, &type_id, route);
    if (route == NULL){
        log_debug("Could not delete route %d -- route not in route set", route_id);
        return -1;
    }

    HASH_DEL(*routes, route);
    destroy_route_set(route);
    log_debug("Removed route %d from set", route_id);
    return 0;
}

struct route_set *get_type_from_route_set(struct route_set **routes, int type_id){
    struct route_set *type_set = NULL;
    HASH_FIND_INT(*routes, &type_id, type_set);
    if (type_set == NULL){
        log_error("No routes available of type %d", type_id);
        for (struct route_set *route = *routes; route != NULL; route=route->hh.next)
            log_debug("Available: %d", route->id);
        return NULL;
    }
    return type_set;
}

int add_route_endpoint(int route_id, struct msu_endpoint *endpoint, uint32_t range_end){
    struct routing_table *table = get_routing_table(route_id);
    return add_routing_table_entry(table, endpoint, range_end);
}

int remove_route_destination(int route_id, int msu_id){
    struct routing_table *table = get_routing_table(route_id);
    struct msu_endpoint *endpoint = rm_routing_table_entry(table, msu_id);
    if (endpoint == NULL){
        log_error("Error removing msu %d from route %d", msu_id, route_id);
        return -1;
    }
    free(endpoint);
    return 0;
}


void remove_destination_from_all_routes(int msu_id){
    // TODO: This is slow. Should only care about routes we're interested in
    for (struct route_set *ref = all_routes; ref != NULL; ref=ref->hh.next){

        struct msu_endpoint *endpoint = rm_routing_table_entry(ref->table, msu_id);
        if (endpoint != NULL){
            free(endpoint);
        }
    }
}

int modify_route_key_range(int route_id, int msu_id, uint32_t new_range_end){
    struct routing_table *table = get_routing_table(route_id);
    int rtn = alter_routing_table_entry(table, msu_id, new_range_end);
    if (rtn < 0){
        log_error("Error altering routing for msu %d on route %d", msu_id, route_id);
        return -1;
    }
    return 0;
}
