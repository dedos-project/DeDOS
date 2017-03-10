#pragma once
/* routing.h
 *
 * Defines routing table structure for each msu
 * and function declarations to modify the routing table
 *
 */

typedef struct msu_control_update msu_control_update;

#include "generic_msu.h"
#include "uthash.h"

enum locality {MSU_IS_LOCAL=1, MSU_IS_REMOTE=2};

typedef struct msu_endpoint{
    int id;
    enum locality locality;
    uint32_t *ipv4; //If remote, IP of remote runtime
} msu_endpoint;

// TODO: Add to generic msu??
typedef struct msu_list {
    local_msu *msu;
    struct msu_list *next;
} msu_list;

typedef struct msu_route {
    int id;
    int n_destinations;
    msu_list *destinations;
    UT_hash_handle hh;
} msu_route;


enum msu_update {
    ADD_ROUTE_ORIGIN,
    ADD_ROUTE_DESTINATION,
    REMOVE_ROUTE_ORIGIN,
    REMOVE_ROUTE_DESTINATION
};

struct msu_control_update{
    local_msu *msu;
    enum msu_update update_type;
    unsigned int payload_len;
    void *payload;
};


int ipv4_to_string(char *ipbuf, const uint32_t ip);
int string_to_ipv4(const char *ipstr, uint32_t *ip);

msu_route *init_route(int type_id);
int destroy_route(int id);
msu_route *get_route(msu_route *routes, int id);
msu_route *add_route(msu_route *routes, int id);

int route_add_destination(msu_route *route, local_msu *destination);
msu_route *route_id_add_destination(int id, local_msu *destination);

int route_rm_destination(msu_route *route, local_msu *destination);
msu_route *route_id_rm_destination(int id, local_msu *destination);

int route_add_origin(msu_route *route, local_msu *origin);
msu_route *route_id_add_origin(int id, local_msu *origin);

int route_rm_origin(msu_route *route, local_msu *origin);
msu_route *route_id_rm_origin(int id, local_msu *origin);


