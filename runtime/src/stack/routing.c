#include "routing.h"

#define DEFAULT_MAX_DESTINATIONS 16

int ipv4_to_string(char *ipbuf, const uint32_t ip)
{
    const unsigned char *addr = (const unsigned char *) &ip;
    int i;

    if (!ipbuf) {
        return -1;
    }

    for (i = 0; i < 4; i++) {
        if (addr[i] > 99) {
            *ipbuf++ = (char) ('0' + (addr[i] / 100));
            *ipbuf++ = (char) ('0' + ((addr[i] % 100) / 10));
            *ipbuf++ = (char) ('0' + ((addr[i] % 100) % 10));
        } else if (addr[i] > 9) {
            *ipbuf++ = (char) ('0' + (addr[i] / 10));
            *ipbuf++ = (char) ('0' + (addr[i] % 10));
        } else {
            *ipbuf++ = (char) ('0' + addr[i]);
        }

        if (i < 3)
            *ipbuf++ = '.';
    }
    *ipbuf = '\0';

    return 0;
}

int string_to_ipv4(const char *ipstr, uint32_t *ip)
{
    unsigned char buf[4] = { 0 };
    int cnt = 0;
    char p;

    if (!ipstr || !ip) {
        return -1;
    }
    while ((p = *ipstr++) != 0 && cnt < 4) {
        if (is_digit(p)) {
            buf[cnt] = (uint8_t) ((10 * buf[cnt]) + (p - '0'));
        } else if (p == '.') {
            cnt++;
        } else {
            return -1;
        }
    }
    /* Handle short notation */
    if (cnt == 1) {
        buf[3] = buf[1];
        buf[1] = 0;
        buf[2] = 0;
    } else if (cnt == 2) {
        buf[3] = buf[2];
        buf[2] = 0;
    } else if (cnt != 3) {
        /* String could not be parsed, return error */
        return -1;
    }

    *ip = long_from(buf);

    return 0;
}


struct routing_table{
    int n_refs;
    
    // For concurrency control
    int n_readers;
    pthread_mutex_t read_mutex;
    pthread_mutex_t write_mutex;

    int n_destinations;
    int max_destinations;
    uint32_t *ranges;
    struct msu_endpoint *destinations;
};

static struct route_reference *all_routes = NULL;

static int realloc_table_entries(struct routing_table *table, int max_entries){
    table->ranges = realloc(table->ranges, sizeof(uint32_t) * max_entries);
    if (table->ranges == NULL){
        log_error("Error allocating table ranges");
        return -1;
    }

    table->destinations = realloc(table->destinations, sizeof(msu_endpoint) * max_entries);
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
    pthread_mutex_lock(&table->write_mutex)
    int index = find_id_index(table, msu_id);
    if (index == -1){
        log_error("MSU %d does not exist in specified route");
        pthread_mutex_unlock(&table->write_mutex);
        return -1;
    }
    struct msu_endpoint *to_remove = &table->destinations[index];
    for (int i=index; i<table->n_destinations-1; i++){
        table->ranges[i] = table->ranges[i+1];
        table->destinations[i] = table->destinations[i+1];
    }
    table->n_destinations--;
    pthread_mutex_unlock(&table->write_mutex);

}
   
static int add_routing_table_entry(struct routing_table *table, 
                                    msu_endpoint *destination, uint32_t range_end){
    pthread_mutex_lock(&table->write_mutex)
    if (table->n_destinations == table->max_destinations){
        int rtn = realloc_table_entries(table, 
                table->max_destinations + DEFAULT_MAX_DESTINATIONS);
        if (rtn < 0){
            log_error("Could not add new route endpoint");
            pthread_mutex_unlock(&table->write_mutex);
            return -1;
        }
    }
    int index = find_value_index(table, range_end);
    for (int i=table->n_destinations; i>index; --i){
        table->ranges[i] = table->ranges[i-1];
        table->destinations[i] = table->destinations[i-1];
    }
    table->ranges[i] = range_end;
    table->destinations[i] = *destination;
    pthread_mutex_unlock(&table->write_mutex);
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
    table->n_refs = 0;
    table->n_destinations = 0;
    table->max_destinations = 0;
    table->n_readers = 0;
    pthread_mutex_init(&table->read_mutex, NULL);
    pthread_mutex_init(&table->write_mutex, NULL);

    int rtn = realloc_table_entries(table, DEFAULT_MAX_DESTINATIONS);
    if (rtn < 0){
        log_error("Error allocating table entry");
        free(table);
        return NULL;
    }

    return table;
}

static struct routing_table *get_routing_table(int route_id){
    struct route_reference *route = malloc(sizeof(*route));
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
        HASH_ADD_INT(all_routes, route_id, route);
    }
    return route->table;
}

struct msu_endpoint *get_route_endpoint(struct route_reference *ref, uint32_t key){
    struct routing_table *table = ref->table;
    pthread_mutex_lock(table->read_mutex);
    int index = find_value_index(table, key);
    if (index < 0){
        log_error("Could not find index for key %d", key);
        pthread_mutex_unlock(table->read_mutex);
        return NULL;
    }
    struct msu_endpoint *endpoint = table->destinations[index];
    pthread_mutex_unlock(table->read_mutex);
    return endpoint;
}

struct route_reference *get_route_ref(int route_id){
    struct routing_table *table = get_routing_table(route_id);
    if (table == NULL){
        log_error("Error getting reference to route");
        return;
    }
    struct route_reference *ref = malloc(sizeof(*route));
    if (ref == NULL){
        log_error("Error allocating new route reference");
    }
    ref->table = table;
    pthread_mutex_lock(&ref->table->write_mutex);
    ++(ref->table->n_refs);
    pthread_mutex_unlock(&ref->table->write_mutex);
}

void destroy_route_ref(struct route_reference *ref){
    pthread_mutex_lock(&ref->table->write_mutex);
    --(ref->table->n_refs);
    free(ref);
    pthread_mutex_unlock(&ref->table->write_mutex);
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
    for (struct route_reference *ref = all_routes;
            ref != NULL;
            ref=ref->hh.next()){
        
        struct msu_endpoint *endpoint = rm_routing_table_entry(ref->table, msu_id);
        if (endpoint != NULL){
            log_debug("Removed msu %d from route %d", msu_id, route_id);
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
