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

int ipv4_to_string(char *ipbuf, const uint32_t ip);
int string_to_ipv4(const char *ipstr, uint32_t *ip);

struct msu_endpoint{
    int id;
    int type_id;
    enum locality locality;
    union{
        uint32_t ipv4; //If remote, IP of remote runtime
        struct generic_msu_queue *msu_queue;
    };
};

// Forward declaration so reads and writes have 
// to take place through the API (to adhere to mutex)
struct routing_table;

struct route_set{
    int id;
    struct routing_table *table;
    UT_hash_handle hh;
};

struct generic_msu_queue *get_msu_queue(int msu_id, int msu_type_id);
struct msu_endpoint *get_route_endpoint(struct route_set *routes, uint32_t key);
struct msu_endpoint *get_endpoint_by_id(struct route_set *routes, int msu_id);

int init_route(int route_id, int type_id);
int modify_route_key_range(int route_id, int msu_id, uint32_t new_range_end);
int add_route_endpoint(int route_id, struct msu_endpoint *endpoint, uint32_t range_end);
int remove_route_destination(int route_id, int msu_id);
void remove_destination_from_all_routes(int msu_id);

struct route_set *get_type_from_route_set(struct route_set **routes, int type_id);
int add_route_to_set(struct route_set **routes, int route_id);
int del_route_from_set(struct route_set **routes, int route_id);
