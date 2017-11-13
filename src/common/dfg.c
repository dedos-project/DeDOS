/**
 * @file dfg.c
 *
 * Code for the creation and modifcation of the data-flow graph
 */
#include "dfg.h"
#include "logging.h"

#include <stdbool.h>
#include <stdlib.h>

/** Static local copy of the DFG, so each call doesn't have to pass a copy */
static struct dedos_dfg *dfg;

void set_dfg(struct dedos_dfg *dfg_in) {
    dfg = dfg_in;
}

/**
 * Convenience macro to search a field within a structure for the element with an ID.
 * Specifically, searches field[i]->id for each element in field.
 * @param s A parent structure. If NULL, returns NULL
 * @param n The number of elements in the field to search
 * @param field The list of structures to search for the given ID
 * @param id_ The ID to search for in the list of structures
 */
#define SEARCH_FOR_ID(s, n, field, id_) \
    if (dfg == NULL) \
        return NULL; \
    for (int i=0; i<(n); ++i) { \
        if (field[i]->id == id_) { \
            return field[i]; \
        } \
    } \
    return NULL;

struct db_info *get_db_info() {
    return &dfg->db;
}

int get_dfg_n_runtimes() {
    return dfg->n_runtimes;
}

struct dfg_runtime *get_dfg_runtime(unsigned int runtime_id) {
    SEARCH_FOR_ID(dfg, dfg->n_runtimes, dfg->runtimes, runtime_id);
}

struct dfg_msu_type *get_dfg_msu_type(unsigned int id){
    SEARCH_FOR_ID(dfg, dfg->n_msu_types, dfg->msu_types, id)
}

struct dfg_route *get_dfg_runtime_route(struct dfg_runtime *rt, unsigned int id) {
    SEARCH_FOR_ID(rt, rt->n_routes, rt->routes, id);
}

struct dfg_route *get_dfg_route(unsigned int id) {
    for (int i=0; i<dfg->n_runtimes; i++) {
        struct dfg_route *route = get_dfg_runtime_route(dfg->runtimes[i], id);
        if (route != NULL) {
            return route;
        }
    }
    return NULL;
}

struct dfg_msu *get_dfg_msu(unsigned int id){
    SEARCH_FOR_ID(dfg, dfg->n_msus, dfg->msus, id)
}

struct dfg_route *get_dfg_rt_route_by_type(struct dfg_runtime *rt, struct dfg_msu_type *type) {
    for (int i=0; i<rt->n_routes; i++) {
        if (rt->routes[i]->msu_type == type) {
            return rt->routes[i];
        }
    }
    return NULL;
}

struct dfg_route *get_dfg_msu_route_by_type(struct dfg_msu *msu, struct dfg_msu_type *route_type) {
    for (int i=0; i<msu->scheduling.n_routes; i++) {
        if (msu->scheduling.routes[i]->msu_type == route_type) {
            return msu->scheduling.routes[i];
        }
    }
    return NULL;
}

struct dfg_thread *get_dfg_thread(struct dfg_runtime *rt, unsigned int id){
    SEARCH_FOR_ID(rt, rt->n_pinned_threads + rt->n_unpinned_threads, rt->threads, id)
}

struct dfg_route_endpoint *get_dfg_route_endpoint(struct dfg_route *route, unsigned int msu_id) {
    for (int i=0; i<route->n_endpoints; i++) {
        if (route->endpoints[i]->msu->id == msu_id) {
            return route->endpoints[i];
        }
    }
    return NULL;
}

int msu_has_route(struct dfg_msu *msu, struct dfg_route *route) {
    for (int i=0; i<msu->scheduling.n_routes; i++) {
        if (msu->scheduling.routes[i] == route) {
            return 1;
        }
    }
    return 0;
}

struct dfg_msu *msu_type_on_runtime(struct dfg_runtime *rt, struct dfg_msu_type *type) {
    for (int i=0; i<type->n_instances; i++) {
        if (type->instances[i]->scheduling.runtime == rt) {
            return type->instances[i];
        }
    }
    return NULL;
}

enum thread_mode str_to_thread_mode(char *mode_str) {
    if (strcasecmp(mode_str, "pinned") == 0) {
        return PINNED_THREAD;
    } else if (strcasecmp(mode_str, "unpinned") == 0) {
        return UNPINNED_THREAD;
    } else {
        log_warn("Unknown thread mode: %s", mode_str);
        return UNSPECIFIED_THREAD_MODE;
    }
}

enum blocking_mode str_to_blocking_mode(char *mode_str) {
    if (strcasecmp(mode_str, "blocking") == 0) {
        return BLOCKING_MSU;
    } else if (strcasecmp(mode_str, "nonblocking") == 0 ||
               strcasecmp(mode_str, "non-blocking") == 0) {
        return NONBLOCK_MSU;
    } else {
        log_warn("Unknown blocking mode: %s", mode_str);
        return UNKNOWN_BLOCKING_MODE;
    }
}

uint8_t str_to_vertex_type(char *str_type) {
    uint8_t vertex_type = 0;
    if (strstr(str_type, "entry") != NULL) {
        vertex_type |= ENTRY_VERTEX_TYPE;
    }
    if (strstr(str_type, "exit") != NULL) {
        vertex_type |= EXIT_VERTEX_TYPE;
    }
    if (vertex_type == 0 && strstr(str_type, "nop") == NULL) {
        log_warn("Unknown vertex type %s specified (neither exit, entry, nop found)", str_type);
        return 0;
    }
    return vertex_type;
}

/**
 * Sets the non-scheduling properties of the MSU to be equal to those of the passed in target
 * @param template The MSU from which to copy the properties
 * @param target The MSU into which the properties are to be copied
 */
static void set_msu_properties(struct dfg_msu *template, struct dfg_msu *target) {
    target->id = template->id;
    target->vertex_type = template->vertex_type;
    target->init_data = template->init_data;
    target->type = template->type;
    target->blocking_mode = template->blocking_mode;
}

struct dfg_msu *copy_dfg_msu(struct dfg_msu *input) {
    struct dfg_msu *msu = calloc(1, sizeof(*input));
    if (msu == NULL) {
        log_error("Error allocating dfg MSU");
        return NULL;
    }
    set_msu_properties(input, msu);
    return msu;
}

struct dfg_msu *init_dfg_msu(unsigned int id, struct dfg_msu_type *type, uint8_t vertex_type,
                             enum blocking_mode mode, struct msu_init_data *init_data) {

    struct dfg_msu *msu = get_dfg_msu(id);
    if (msu != NULL) {
        log_error("MSU %u already exists and scheduled", id);
        return NULL;
    }

    msu = calloc(1, sizeof(*msu));
    if (msu == NULL) {
        log_error("Error allocating dfg MSU");
        return NULL;
    }
    struct dfg_msu template = {
        .id = id,
        .vertex_type = vertex_type,
        .init_data = *init_data,
        .type = type,
        .blocking_mode = mode
    };
    set_msu_properties(&template, msu);
    return msu;
}

int free_dfg_msu(struct dfg_msu *input) {
    if (input->scheduling.runtime != NULL || input->scheduling.thread != NULL) {
        log_error("Cannot free MSU that is still assigned to thread or runtime");
        return -1;
    }
    free(input);
    return 0;
}

/**
 * Adds the MSU to the thread, runtime, and instance, and adds the thread and runtime to the MSU
 */
static int schedule_msu_on_thread(struct dfg_msu *msu, struct dfg_thread *thread,
                                  struct dfg_runtime *rt) {
    if (thread->n_msus == MAX_MSU_PER_THREAD) {
        log_error("Too many MSUs on thread %d", thread->id);
        return -1;
    }
    if (dfg->n_msus == MAX_MSU) {
        log_error("Too many MSUs in DFG");
        return -1;
    }
    dfg->msus[dfg->n_msus] = msu;
    dfg->n_msus++;

    thread->msus[thread->n_msus] = msu;
    thread->n_msus++;

    msu->type->instances[msu->type->n_instances] = msu;
    msu->type->n_instances++;

    msu->scheduling.thread = thread;
    msu->scheduling.runtime = rt;
    return 0;
}

int schedule_dfg_msu(struct dfg_msu *msu, unsigned int runtime_id, unsigned int thread_id) {
    if (dfg == NULL) {
        log_error("DFG not instantiated");
        return -1;
    }

    if (msu->scheduling.runtime != NULL || msu->scheduling.thread != NULL) {
        log_error("MSU already scheduled");
        return -1;
    }

    struct dfg_runtime *rt = get_dfg_runtime(runtime_id);
    if (rt == NULL) {
        log_error("Runtime id %u not instantiated", runtime_id);
        return -1;
    }
    struct dfg_thread *thread = get_dfg_thread(rt, thread_id);
    if (thread == NULL) {
        log_error("Thread id %u not instantiated on runtime %u", thread_id, runtime_id);
        return -1;
    }

    int rtn = schedule_msu_on_thread(msu, thread, rt);
    if (rtn < 0) {
        log_error("Error scheduling MSU on thread");
        return -1;
    }

    log_info("TEST");
    log(LOG_DFG, "Scheduled DFG msu %d", msu->id);
    return 0;
}

/**
 * Removes the msu from it's thread, runtime, and instance,
 *  and removes the thread and runtime from the MSU
 */
static int remove_msu_from_thread(struct dfg_msu *msu) {
    struct dfg_thread *thread = msu->scheduling.thread;
    int ti;
    for (ti=0; ti<thread->n_msus && thread->msus[ti] != msu; ti++){}
    if (ti == thread->n_msus) {
        log_error("MSU %d does not exist on thread %d", msu->id, thread->id);
        return -1;
    }
    int di;
    for (di = 0; di < dfg->n_msus && dfg->msus[di] != msu; di++){}
    if (di == dfg->n_msus) {
        log_critical("Cannot find MSU %d in DFG!", msu->id);
        return -1;
    }
    int ii;
    for (ii = 0; ii < msu->type->n_instances && msu->type->instances[ii] != msu; ii++) {}
    if (ii == msu->type->n_instances) {
        log_error("Cannot find MSU %d in list of instances of %s", msu->id, msu->type->name);
        return -1;
    }

    for (int i=ti; i < thread->n_msus - 1; i++) {
        thread->msus[i] = thread->msus[i+1];
    }
    thread->n_msus--;

    for (int i=di; i < dfg->n_msus - 1; i++) {
        dfg->msus[i] = dfg->msus[i+1];
    }
    dfg->n_msus--;

    for (int i=ii; i < msu->type->n_instances - 1; i++) {
        msu->type->instances[i] = msu->type->instances[i+1];
    }
    msu->type->n_instances--;

    msu->scheduling.runtime = NULL;
    msu->scheduling.thread = NULL;
    return 0;
}

int unschedule_dfg_msu(struct dfg_msu *msu) {
    if (dfg == NULL) {
        log_error("DFG not instantiated");
        return -1;
    }

    if (msu->scheduling.runtime == NULL || msu->scheduling.thread == NULL) {
        log_error("MSU not scheduled");
        return -1;
    }

    for (int i=0; i<dfg->n_runtimes; i++) {
        struct dfg_runtime *rt = dfg->runtimes[i];
        for (int j=0; j<rt->n_routes; j++) {
            struct dfg_route *route = rt->routes[j];
            if (get_dfg_route_endpoint(route, msu->id) != NULL) {
                log_error("MSU %d Cannot be unscheduled: referenced by route %d",
                          msu->id, route->id);
                return -1;
            }
        }
    }

    log(LOG_DFG, "Unscheduled DFG msu %d", msu->id);
    return remove_msu_from_thread(msu);
}

struct dfg_route *create_dfg_route(unsigned int id, struct dfg_msu_type *type,
                                   unsigned int runtime_id) {
    struct dfg_route *route = get_dfg_route(id);
    if (route != NULL) {
        log_error("DFG route %u already exists", id);
        return NULL;
    }
    struct dfg_runtime *rt = get_dfg_runtime(runtime_id);
    if (rt == NULL) {
        log_error("Runtime %u does not exist in DFG", runtime_id);
        return NULL;
    }
    if (rt->n_routes == MAX_ROUTES) {
        log_error("Cannot fit more routes on runtime %u", runtime_id);
        return NULL;
    }

    route = calloc(1, sizeof(*route));
    if (route == NULL) {
        log_error("Could not allocate DFG route %u", id);
        return NULL;
    }

    route->id = id;
    route->msu_type = type;
    route->runtime = rt;
    // TODO: Add runtime to route in dfg_reader

    rt->routes[rt->n_routes] = route;
    rt->n_routes++;
    return route;
}

int delete_dfg_route(struct dfg_route *route) {
    for (int i=0; i<dfg->n_msus; i++) {
        struct dfg_msu *msu = dfg->msus[i];
        for (int j=0; j<msu->scheduling.n_routes; j++) {
            if (msu->scheduling.routes[i] == route) {
                log_error("Cannot delete route %d while it is still attached to msu %d",
                          route->id, msu->id);
                return -1;
            }
        }
    }

    struct dfg_runtime *rt = route->runtime;

    int i;
    // Find the index of the route in the runtime
    for (i=0; i<rt->n_routes && rt->routes[i] != route; i++);
    if (i == rt->n_routes) {
        log_error("Could not find route in runtime");
        return -1;
    }
    for (; i<rt->n_routes-1; i++) {
        rt->routes[i] = rt->routes[i+1];
    }
    rt->n_routes--;
    return 0;
}


int add_dfg_route_to_msu(struct dfg_route *route, struct dfg_msu *msu) {
    if (msu->scheduling.runtime != route->runtime) {
        log_error("Cannot add route on runtime %d to msu on runtime %d",
                  route->runtime->id, msu->scheduling.runtime->id);
        return -1;
    }
    for (int i=0; i<msu->scheduling.n_routes; i++) {
        if (msu->scheduling.routes[i]->msu_type == route->msu_type) {
            log_error("Route with type %d already exists in MSU %d",
                      route->msu_type->id, msu->id);
            return -1;
        }
    }

    if (route->n_endpoints == 0) {
        log_warn("Route with no destinations added to an MSU! "
                 "(Route: %d, msu: %d) "
                 "This could have unfortunate results!",
                 (int)route->id, (int)msu->id);
    }

    msu->scheduling.routes[msu->scheduling.n_routes] = route;
    msu->scheduling.n_routes++;
    return 0;
}


struct dfg_route_endpoint *add_dfg_route_endpoint(struct dfg_msu *msu, uint32_t key,
                                                  struct dfg_route *route) {
    if (route->n_endpoints == MAX_ROUTE_ENDPOINTS) {
        log_error("Cannot add more MSUs to route %d", route->id);
        return NULL;
    }
    if (route->msu_type != msu->type) {
        log_error("Cannot add MSU of type %d to route of type %d",
                  msu->type->id, route->msu_type->id);
        return NULL;
    }

    struct dfg_route_endpoint *dest = malloc(sizeof(*dest));
    if (dest == NULL) {
        log_perror("Error allocating route destination");
        return NULL;
    }
    dest->msu = msu;
    dest->key = key;

    int i;
    // Insert the endpoint in the correct place (ascending keys)
    for (i=0; i<route->n_endpoints && route->endpoints[i]->key < key; i++);
    for (int j=route->n_endpoints; j>i; j--) {
        route->endpoints[j] = route->endpoints[j-1];
    }
    route->endpoints[i] = dest;
    route->n_endpoints++;

    return dest;
}

int del_dfg_route_endpoint(struct dfg_route *route, struct dfg_route_endpoint *ep) {
    int i;
    // Search for endpoint index
    for (i=0; i<route->n_endpoints && route->endpoints[i] != ep; i++);

    if (i == route->n_endpoints) {
        log_error("Could not find endpoint in route");
        return -1;
    }

    for (; i<route->n_endpoints-1; i++) {
        route->endpoints[i] = route->endpoints[i+1];
    }
    route->n_endpoints--;
    log(LOG_ROUTING_CHANGES, "Route %d now has %d endpoints", route->id, route->n_endpoints);
    return 0;
}

int mod_dfg_route_endpoint(struct dfg_route *route, struct dfg_route_endpoint *ep,
                           uint32_t new_key) {
    // This is all so the keys remain in ascending order

    // Find where the endpoint used to be
    int old;
    for (old=0; old<route->n_endpoints && route->endpoints[old] != ep; old++);
    if (old == route->n_endpoints) {
        log_error("Could not find endpoint in route");
        return -1;
    }
    // Find where it should now be
    int new;
    for (new=0; new<route->n_endpoints && route->endpoints[new]->key < new_key; new++);

    // Which direction are we moving the in-between endpoints
    int delta = new > old ? 1 : -1;
    // If we're deleting from below the new location, subtract one to the new location
    if (delta == 1) {
        new -= 1;
    }

    // Iterate from the old to the new location, moving the endpoints in the other direction
    for (int i=old; delta * i < delta * new; i += delta) {
        route->endpoints[i] = route->endpoints[i + delta];
    }


    // Replace the new endpoint
    ep->key = new_key;
    route->endpoints[new] = ep;
    return 0;
}

struct dfg_thread * create_dfg_thread(struct dfg_runtime *rt, int thread_id,
                                      enum thread_mode mode) {
    if (mode == UNSPECIFIED_THREAD_MODE) {
        log_error("Cannot add unspecified-thread-mode thread");
        return NULL;
    }
    if (rt->n_pinned_threads + rt->n_unpinned_threads == MAX_THREADS) {
        log_error("Cannot add more threads to runtime %d", rt->id);
        return NULL;
    }

    struct dfg_thread *thread = calloc(1, sizeof(*thread));
    if (thread == NULL) {
        log_error("Could not allocate thread");
        return NULL;
    }
    thread->id = thread_id;
    thread->mode = mode;

    rt->threads[rt->n_pinned_threads + rt->n_unpinned_threads] = thread;
    switch(mode) {
        case PINNED_THREAD:
            rt->n_pinned_threads++;
            break;
        case UNPINNED_THREAD:
            rt->n_unpinned_threads++;
            break;
        default:
            log_error("Unknown thread mode %d provided", mode);
            return NULL;
    }
    return thread;
}

/** Frees elements in the MSU type structure */
static void free_dfg_msu_type(struct dfg_msu_type *type) {
    for (int i=0; i<type->n_dependencies; i++) {
        free(type->dependencies[i]);
    }
    free(type);
}

/** Frees the runtime and all threads and routes associated with it */
static void free_dfg_runtime(struct dfg_runtime *rt) {
    for (int i=0; i < rt->n_pinned_threads + rt->n_unpinned_threads; i++) {
        free(rt->threads[i]);
    }
    for (int i=0; i < rt->n_routes; i++) {
        for (int j=0; j < rt->routes[i]->n_endpoints; j++) {
            free(rt->routes[i]->endpoints[j]);
        }
        free(rt->routes[i]);
    }
    free(rt);
}


void free_dfg(struct dedos_dfg *dfg) {
    for (int i=0; i < dfg->n_msu_types; i++) {
        free_dfg_msu_type(dfg->msu_types[i]);
    }
    for (int i=0; i < dfg->n_msus; i++) {
        free(dfg->msus[i]);
    }
    for (int i=0; i < dfg->n_runtimes; i++) {
        free_dfg_runtime(dfg->runtimes[i]);
    }
    free(dfg);
}
