#pragma once
/* routing.h
 *
 * Defines routing table structure for each msu
 * and function declarations to modify the routing table
 *
 */

typedef struct msu_control_update msu_control_update_t;

#include "generic_msu.h"
#include "uthash.h"

enum locality_t {MSU_IS_LOCAL=1, MSU_IS_REMOTE=2};

typedef struct msu_endpoint_t{
    int id;
    enum locality_t locality;
    uint32_t *ipv4; //If remote, IP of remote runtime
} msu_endpoint_t;

// TODO: Add to generic msu??
typedef struct msu_list_t {
    msu_t *msu;
    struct msu_list_t *next;
} msu_list_t;

typedef struct msu_route_t {
    int id;
    int n_destinations;
    msu_list_t *destinations;
    UT_hash_handle hh;
} msu_route_t;


enum msu_update_t {
    ADD_ROUTE_ORIGIN,
    ADD_ROUTE_DESTINATION,
    REMOVE_ROUTE_ORIGIN,
    REMOVE_ROUTE_DESTINATION
};

struct msu_control_update{
    msu_t *msu;
    enum msu_update_t update_type;
    unsigned int payload_len;
    void *payload;
};


int ipv4_to_string(char *ipbuf, const uint32_t ip);
int string_to_ipv4(const char *ipstr, uint32_t *ip);

msu_route_t *init_route(int type_id);
int destroy_route(int id);
msu_route_t *get_route(msu_route_t *routes, int id);
msu_route_t *add_route(msu_route_t *routes, int id);

int route_add_destination(msu_route_t *route, msu_t *destination);
msu_route_t *route_id_add_destination(int id, msu_t *destination);

int route_rm_destination(msu_route_t *route, msu_t *destination);
msu_route_t *route_id_rm_destination(int id, msu_t *destination);

int route_add_origin(msu_route_t *route, msu_t *origin);
msu_route_t *route_id_add_origin(int id, msu_t *origin);

int route_rm_origin(msu_route_t *route, msu_t *origin);
msu_route_t *route_id_rm_origin(int id, msu_t *origin);


