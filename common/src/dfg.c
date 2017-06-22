#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#include "dfg.h"
#include "logging.h"
#include "ip_utils.h"
#include "control_protocol.h"

/* Display information about runtimes */
int show_connected_peers(void) {
    char ipstr[INET_ADDRSTRLEN];
    int count = 0;
    int i;

    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    debug("INFO: %s", "Current open runtime peer sockets ->");

    for (i = 0; i < dfg->runtimes_cnt; ++i) {
        if (dfg->runtimes[i]->sock != -1) {
            ipv4_to_string(ipstr, dfg->runtimes[i]->ip);
            debug("INFO => Socket: %d, IP: %s, Cores: %u, Pinned Threads: %u  Total Threads: %u",
                  dfg->runtimes[i]->sock,
                  ipstr,
                  dfg->runtimes[i]->num_cores,
                  dfg->runtimes[i]->num_pinned_threads,
                  dfg->runtimes[i]->num_threads);
            count++;
        }
    }

    return count;
}

/**
 * Get runtime pointer from ID
 * @param integer runtime_id
 * @return struct dfg_runtime_endpoint *runtime
 */
struct dfg_runtime_endpoint *get_runtime_from_id(int runtime_id) {
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    int i;
    for (i = 0; i < dfg->runtimes_cnt; ++i) {
        if (dfg->runtimes[i]->id == runtime_id) {
            return dfg->runtimes[i];
        }
    }

    return NULL;
}

/**
 * Get runtimes IPs
 * @param uint32_t *peer_ips
 * @return void
 */
void get_connected_peer_ips(uint32_t *peer_ips) {
    int count = 0;
    int i;
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    for (i = 0; i < dfg->runtimes_cnt; ++i) {
        if (dfg->runtimes[i]->sock != -1) {
            peer_ips[count] = dfg->runtimes[i]->ip;
            count++;
        }
    }

}

/**
 * Get runtime IP from socket
 * @param integer sock
 * @return uint32_t runtime ip
 */
uint32_t get_sock_endpoint_ip(int sock) {
    int i;
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    for (i = 0; i < dfg->runtimes_cnt; ++i) {
        if (dfg->runtimes[i]->sock == sock) {
            return dfg->runtimes[i]->ip;
        }
    }

    //TODO answer an unsigned integer here (0?)
    return -1;
}

/**
 * Get runtime index from socket
 * @param integer sock
 * @return integer runtime index
 */
int get_sock_endpoint_index(int sock) {
    int i;
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    for (i = 0; i < dfg->runtimes_cnt; ++i) {
        if (dfg->runtimes[i]->sock == sock) {
            return i;
        }
    }

    return -1;
}


/**
 * Retrieve all MSU ids of a given type
 * @param integer type
 * @return struct msus_of_type *msu_of_type
 */
struct msus_of_type *get_msus_from_type(int type) {
    int *msu_ids = malloc(sizeof(int) * MAX_MSU);
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();
    struct msus_of_type *s = NULL;
    s = malloc(sizeof(struct msus_of_type));

    int p = 0;
    int c;
    for (c = 0; c < dfg->vertex_cnt; ++c) {
        if (dfg->vertices[c]->msu_type == type) {
            msu_ids[p] = dfg->vertices[c]->msu_id;
            p++;
        }
    }

    s->msu_ids = msu_ids;
    s->num_msus = p;
    s->type = type;

    return s;
}

/**
 * Retrieve dfg vertex given a msu ID
 * @param integer msu_id
 * @return struct dfg_vertex *msu
 */
struct dfg_vertex *get_msu_from_id(int msu_id) {
    struct dfg_vertex *msu = NULL;
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    int i;
    for (i = 0; i < dfg->vertex_cnt; ++i) {
        if (dfg->vertices[i]->msu_id == msu_id) {
            msu = dfg->vertices[i];
            break;
        }
    }

    return msu;
}

/**
 * Retrieve dfg route given a runtime socket number and route ID
 * @param rt runtime that contains the route ID
 * @param route_id id of the route to retrieve
 * @return dfg route if successful else NULL
 */
struct dfg_route *get_route_from_id(struct dfg_runtime_endpoint *rt, int route_id) {
    for (int i = 0; i < rt->num_routes; i++) {
        struct dfg_route *route = rt->routes[i];
        if ( route->route_id == route_id ) {
            return route;
        }
    }
    return NULL;
}

/* Overwrite or create a MSU in the DFG */
void set_msu(struct dfg_vertex *msu) {
    debug("DEBUG: adding msu %d to the graph", msu->msu_id);

    struct dfg_config *dfg = get_dfg();

    int new_id = -1;

    int m;
    for (m = 0; m < dfg->vertex_cnt; ++m) {
        if (dfg->vertices[m]->msu_id == msu->msu_id) {
            free(dfg->vertices[m]);
            new_id = m;
            break;
        }
    }

    new_id = (new_id != -1)? new_id : dfg->vertex_cnt;
    dfg->vertices[new_id] = msu;
    dfg->vertex_cnt++;

    memset(&msu->statistics, 0, sizeof(msu->statistics));
}

/**
 * Adds an outgoing route to an MSU
 * @param runtime_sock runtime on which the route and msu reside
 * @param msu_id msu to which the route is to be added
 * @param route_id ID of the route to add to the msu (must already exist)
 * @return 0 on success, -1 on error
 */
int add_route_to_msu_vertex(int runtime_index, int msu_id, int route_id) {
    struct dfg_config *dfg = get_dfg();

    struct dfg_runtime_endpoint *rt = dfg->runtimes[runtime_index];

    struct dfg_vertex *msu = get_msu_from_id(msu_id);
    if (msu == NULL){
        log_error("Specified MSU %d does not exist", msu_id);
        return -1;
    }
    if ( get_sock_endpoint_index(msu->scheduling.runtime->sock) != runtime_index ) {
        log_error("Specified MSU %d does not reside on runtime %d", msu_id, runtime_index);
        return -1;
    }

    struct dfg_route *route = get_route_from_id(rt, route_id);
    if (route == NULL){
        log_error("Specified route %d does not reside on runtime %d", route_id, runtime_index);
        return -1;
    }

    msu->scheduling.routes[msu->scheduling.num_routes] = route;
    msu->scheduling.num_routes++;
    return 0;
}

int del_route_from_msu_vertex(int runtime_index, int msu_id, int route_id) {
    struct dfg_vertex *msu = get_msu_from_id(msu_id);
    if (msu == NULL){
        log_error("Specified MSU %d does not exist", msu_id);
        return -1;
    }
    if ( get_sock_endpoint_index(msu->scheduling.runtime->sock) != runtime_index ) {
        log_error("Specified MSU %d does not reside on runtime %d", msu_id, runtime_index);
        return -1;
    }

    struct msu_scheduling *sched = &msu->scheduling;
    int i;
    for (i=0; i<sched->num_routes; i++){
        struct dfg_route *route = sched->routes[i];
        if (route->route_id == route_id){
            break;
        }
    }

    if ( i == sched->num_routes ) {
        log_error("Specified route %d not assigned to msu %d", route_id, msu_id);
        return -1;
    }

    // Move the remainder of the routes back in the array to fill the gap
    for (; i<sched->num_routes-1; i++){
        sched->routes[i] = sched->routes[i+1];
    }

    sched->num_routes--;
    return 0;
}

/**
 * Add a new route to a runtime
 * @param dfg_runtime_endpoint the target runtime
 * @param route_id: id of the route to be created
 * @param msu_type: destination type for the route
 * @return 0/-1: success/failure
 */
int dfg_add_route(struct dfg_runtime_endpoint *rt, int route_id, int msu_type) {
    struct dfg_route *route = malloc(sizeof(*route));
    if (route == NULL) {
        debug("Could not allocate memory for dfg_route object");
        return -1;
    }

    route->route_id = route_id;
    route->msu_type = msu_type;
    route->num_destinations = 0;
    bzero(route->destination_keys, sizeof(int) * MAX_DESTINATIONS);
    rt->routes[rt->num_routes] = route;
    rt->num_routes++;

    return 0;
}

int dfg_add_route_endpoint(int runtime_index, int route_id, int msu_id, unsigned int range_end){
    struct dfg_config *dfg = get_dfg();
    struct dfg_runtime_endpoint *rt = dfg->runtimes[runtime_index];

    struct dfg_vertex *msu = get_msu_from_id(msu_id);
    if (msu == NULL){
        log_error("Specified MSU %d does not exist", msu_id);
        return -1;
    }

    struct dfg_route *route = get_route_from_id(rt, route_id);
    if (route == NULL){
        dfg_add_route(rt, route_id, msu->msu_type);
        route = get_route_from_id(rt, route_id);
    }

    // Iterate through backwards, advancing entries in the routing table until
    // the correct place for range_end is found
    int i;
    for (i = route->num_destinations; i > 0 && range_end < route->destination_keys[i]; i--){
        route->destinations[i] = route->destinations[i-1];
        route->destination_keys[i] = route->destination_keys[i-1];
    }
    route->destinations[i] = msu;
    route->destination_keys[i] = range_end;
    route->num_destinations++;
    return 0;
}

int dfg_del_route_endpoint(int runtime_index, int route_id, int msu_id){
    struct dfg_config *dfg = get_dfg();
    struct dfg_runtime_endpoint *rt = dfg->runtimes[runtime_index];

    struct dfg_route *route = get_route_from_id(rt, route_id);
    if (route == NULL){
        log_error("Specified route %d does not reside on runtime %d", route_id, runtime_index);
        return -1;
    }

    struct dfg_vertex *msu = get_msu_from_id(msu_id);
    if (msu == NULL){
        log_error("Specified MSU %d does not exist", msu_id);
        return -1;
    }

    // Remove endpoint, move other endpoints backwards to fill gap
    int i;
    for (i=0; i<route->num_destinations; i++){
        if (route->destinations[i]->msu_id == msu_id){
            break;
        }
    }

    if ( i == route->num_destinations ) {
        log_error("Specified msu %d not assigned to route %d", msu_id, route_id);
        return -1;
    }

    // Move everything else backwards
    for (; i<route->num_destinations-1; i++){
        route->destinations[i] = route->destinations[i+1];
        route->destination_keys[i] = route->destination_keys[i+1];
    }

    route->num_destinations--;
    return 0;
}

/**
 * Lookup a route going from a given MSU toward a given type
 * @param struct dfg_vertex msu: the given MSU
 * @param msu_type: the target MSU type
 * @return struct dfg_route: the route object. NULL if not found.
 */
struct dfg_route *get_route_from_type(struct dfg_runtime_endpoint *rt, int msu_type) {
    int i;
    for (i = 0; i < rt->num_routes; ++i) {
        if (rt->routes[i]->msu_type == msu_type) {
            return rt->routes[i];
        }
    }

    return NULL;
}

/**
 * Check whether a route has a specific MSU as endpoint
 * @param struct dfg_route: the target route
 * @param dfg_vertex: a given MSU
 * @return 0/1: false/true
 */
int route_has_endpoint(struct dfg_route *route, struct dfg_vertex *msu) {
    int i;
    for (i = 0; i < route->num_destinations; ++i) {
        if (route->destinations[i] == msu) {
            return 1;
        }
    }

    return 0;
}

/**
 * Check whether a route is attached to a given MSU
 * @param dfg_vertex: the target MSU
 * @param route_id: the target route
 * @return 0/1: false/true
 */
int msu_has_route(struct dfg_vertex *msu, int route_id) {
    int has_route = 0;
    int i;
    for (i = 0; i < msu->scheduling.num_routes; ++i) {
        if (msu->scheduling.routes[i]->route_id == route_id) {
            has_route = 1;
        }
    }

    return has_route;
}

/**
 * Parse a given MSU dependencies to retrieve a type dependency
 * @param: struct dfg_vertex msu: the target MSU
 * @param msu_type: the potential MSU type dependency
 * @return struct dependent_type: the dependency
 */
struct dependent_type *get_dependent_type(struct dfg_vertex *msu, int msu_type) {
    int i;
    for (i = 0; i < msu->num_dependencies; ++i) {
        if (msu->dependencies[i]->msu_type == msu_type) {
            return msu->dependencies[i];
        }
    }

    return NULL;
}

/**
 * Lookup the presence of an instance of an MSU of a given type on a runtime
 * @param struct dfg_runtime_endpoint: runtime to inquire
 * @param msu_type: target MSU type
 * @return 0/1: false/true
 */
int lookup_type_on_runtime(struct dfg_runtime_endpoint *rt, int msu_type) {
    //TODO: Keep count of num_msus in a runtime_thread, so that we can
    //      simply parse the runtime threads and no the whole DFG
    struct dfg_config *dfg = get_dfg();
    int i;
    for (i = 0; i < dfg->vertex_cnt; ++i) {
        if (dfg->vertices[i]->scheduling.runtime == rt &&
            dfg->vertices[i]->msu_type == msu_type) {
            return 1;
        }
    }

    return 0;
}

/**
 * Pick static data (meta routing profiling, etc) from an MSU of similar tye.
 * FIXME: If we find another "empty shell" MSU, we will get wrong data. We expect all MSU of
 * a given type to contain the same static data.
 * @param struct dfg_vertex: MSU to fill
 * @return none
 */
void clone_type_static_data(struct dfg_vertex *msu) {
    struct msus_of_type *peer_msus = get_msus_from_type(msu->msu_type);
    int i;
    for (i = 0; i < peer_msus->num_msus; ++i) {
        if (peer_msus->msu_ids[i] != msu->msu_id) {
            struct dfg_vertex *peer_msu = get_msu_from_id(peer_msus->msu_ids[i]);
            memcpy(&msu->profiling, &peer_msu->profiling, sizeof(struct msu_profiling));
            memcpy(&msu->meta_routing, &peer_msu->meta_routing, sizeof(struct msu_meta_routing));
            memcpy(&msu->dependencies, &peer_msu->dependencies,
                   sizeof(struct dependent_type) * peer_msu->num_dependencies);
            msu->num_dependencies = peer_msu->num_dependencies;

            memcpy(&msu->msu_mode, &peer_msu->msu_mode, strlen(&peer_msu->msu_mode));
        }
    }
}

/**
 * Increment the maximum range of a given route (non commit)
 * @param struct dfg_route route: the target route
 */

int increment_max_range(struct dfg_route *route) {
    int max_range = 0;
    int i;
    for (i = 0; i < route->num_destinations; ++i) {
        if (max_range == -1 || route->destination_keys[i] > max_range) {
            max_range = route->destination_keys[i];
        }
    }

    return max_range + 1;
}

/**
 * Generate a new route id on a given runtime
 * @param struct dfg_runtime_endpoint: pointer to the target runtime
 * @return route_id: the newly generated route id
 */
int generate_route_id(struct dfg_runtime_endpoint *rt) {
    int highest_id = 0;
    int i;
    for (i = 0; i < rt->num_routes; ++i) {
        if (highest_id == -1 || rt->routes[i]->route_id > highest_id) {
            highest_id = rt->routes[i]->route_id;
        }
    }

    return highest_id + 1;
}

/**
 * Generate a new ID
 * @return msu_id: the newly generated MSU id
 */
int generate_msu_id() {
    struct dfg_config *dfg = get_dfg();
    int highest_id = -1;

    int i;
    for (i = 0; i < dfg->vertex_cnt; ++i) {
        if (highest_id == -1 || dfg->vertices[i]->msu_id > highest_id) {
            highest_id = dfg->vertices[i]->msu_id;
        }
    }

    return highest_id + 1;
}

/* Big mess of create, updates, etc */
void update_dfg(struct dedos_dfg_manage_msg *update_msg) {
    debug("DEBUG: updating MSU for action: %d", update_msg->msg_code);

    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    switch (update_msg->msg_code) {
        case RUNTIME_ENDPOINT_ADD: {
            //because both in toload and preload mode we have already been adding some
            //runtimes in the DFG, we need to check if the new runtime isn't actually
            //an "expected" one, while allowing totally new runtimes to register.

            if (dfg->runtimes_cnt == MAX_RUNTIMES) {
                debug("ERROR: %s", "Number of runtimes limit reached.");
                break;
            }

            struct dedos_dfg_add_endpoint_msg *update;
            update = (struct dedos_dfg_add_endpoint_msg *) update_msg->payload;

            debug("DEBUG: registering endpoint at %s", update->runtime_ip);

            struct dfg_runtime_endpoint *r = NULL;

            int i;
            for (i = 0; i < dfg->runtimes_cnt; ++i) {
                char runtime_ip[INET_ADDRSTRLEN];
                ipv4_to_string(runtime_ip, dfg->runtimes[i]->ip);

                //We assume only one runtime per ip
                if (strcmp(runtime_ip, update->runtime_ip) == 0) {
                    r = dfg->runtimes[i];
                    break;
                }
            }

            if (r == NULL) {
                r = malloc(sizeof(struct dfg_runtime_endpoint));

                string_to_ipv4(update->runtime_ip, &(r->ip));

                dfg->runtimes[dfg->runtimes_cnt] = r;
                dfg->runtimes_cnt++;
            }

            r->sock = update->runtime_sock;

            free(update_msg);
            free(update);
            break;
        }

        default:
            debug("DEBUG: unrecognized update action: %d", update_msg->msg_code);
            break;
    }
}
