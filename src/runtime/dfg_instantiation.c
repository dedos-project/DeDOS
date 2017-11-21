/**
 * @file dfg_instantiation.c
 *
 * Instantiation of a dfg on a runtime
 */
#include "logging.h"
#include "local_msu.h"
#include "dfg.h"
#include "routing.h"

/**
 * Adds the provided endpoints to the route with the provided `route_id`
 */
static int add_dfg_route_endpoints(int route_id,
                                      struct dfg_route_endpoint **endpoints,
                                      int n_endpoints) {
    for (int i=0; i<n_endpoints; i++) {
        struct msu_endpoint endpoint;
        struct dfg_route_endpoint *dest = endpoints[i];

        int rtn = init_msu_endpoint(dest->msu->id,
                                    dest->msu->scheduling.runtime->id,
                                    &endpoint);
        if (rtn < 0) {
            log_error("Error instantiating MSU %d endpoint for route %d",
                      dest->msu->id, route_id);
            return -1;
        }
        rtn = add_route_endpoint(route_id, endpoint, dest->key);
        if (rtn < 0) {
            log_error("Error adding endpoint %d to route %d",
                      dest->msu->id, route_id);
            return -1;
        }
    }
    log(LOG_DFG_INSTANTIATION, "Added %d endpoints to route %d",
               n_endpoints, route_id);
    return 0;
}

/**
 * Creates the given routes on the runtime
 */
static int spawn_dfg_routes(struct dfg_route **routes, int n_routes) {
    for (int i=0; i<n_routes; i++) {
        struct dfg_route *dfg_route = routes[i];
        if (init_route(dfg_route->id, dfg_route->msu_type->id) != 0) {
            log_error("Could not instantiate route %d (type: %d)",
                      dfg_route->id, dfg_route->msu_type->id);
            return -1;
        }
    }
    log(LOG_DFG_INSTANTIATION, "Spawned %d routes", n_routes);
    return 0;
}

/**
 * Adds all of the endpoints for the provided routes to the provided routes
 */
static int add_all_dfg_route_endpoints(struct dfg_route **routes, int n_routes) {
    for (int i=0; i<n_routes; i++) {
        struct dfg_route *dfg_route = routes[i];
        if (add_dfg_route_endpoints(dfg_route->id,
                                    dfg_route->endpoints,
                                    dfg_route->n_endpoints) != 0) {
            log_error("Error adding endpoints");
            return -1;
        }
    }
    return 0;
}

/**
 * Subscribes the MSU to the provided routes
 */
static int add_dfg_routes_to_msu(struct local_msu *msu, struct dfg_route **routes, int n_routes) {
    for (int i=0; i<n_routes; i++) {
        struct dfg_route *route = routes[i];
        if (add_route_to_set(&msu->routes,  route->id) != 0) {
            log_error("Error adding route %d to msu %d", route->id, msu->id);
            return -1;
        }
    }
    log(LOG_DFG_INSTANTIATION, "Added %d routes to msu %d",
               n_routes, msu->id);
    return 0;
}

/**
 * Creates all of the MSUs on the provided worker thread
 */
static int spawn_dfg_msus(struct worker_thread *thread, struct dfg_msu **msus, int n_msus) {
    for (int i=0; i<n_msus; i++) {
        struct dfg_msu *dfg_msu = msus[i];
        struct msu_type *type = get_msu_type(dfg_msu->type->id);
        if (type == NULL) {
            log_error("Could not retrieve MSU type %d", dfg_msu->type->id);
            return -1;
        }
        struct local_msu *msu = init_msu(dfg_msu->id, type, thread, &dfg_msu->init_data);
        if (msu == NULL) {
            log_error("Error instantiating MSU %d", dfg_msu->type->id);
            return -1;
        }
        log(LOG_DFG_INSTANTIATION, "Initialized MSU %d from dfg", dfg_msu->id);
        struct dfg_scheduling *sched = &dfg_msu->scheduling;
        if (add_dfg_routes_to_msu(msu, sched->routes, sched->n_routes) != 0) {
            log_error("Error adding routes to MSU %d", msu->id);
            return -1;
        }
    }
    log(LOG_DFG_INSTANTIATION, "Initialized %d MSUs", n_msus);
    return 0;
}

/**
 * Creates all of the provided threads (including MSUs) on the current runtime
 */
static int spawn_dfg_threads(struct dfg_thread **threads, int n_threads) {
    for (int i=0; i<n_threads; i++) {
        struct dfg_thread *dfg_thread = threads[i];
        int rtn = create_worker_thread(dfg_thread->id, dfg_thread->mode);
        if (rtn < 0) {
            log_error("Error instantiating thread %d! Can not continue!", dfg_thread->id);
            return -1;
        }
        log(LOG_DFG_INSTANTIATION, "Created worker thread %d, mode = %s",
                   dfg_thread->id, dfg_thread->mode == PINNED_THREAD ? "Pinned" : "Unpinned");
        struct worker_thread *thread = get_worker_thread(dfg_thread->id);
        if (spawn_dfg_msus(thread, dfg_thread->msus, dfg_thread->n_msus) != 0) {
            log_error("Error instantiating thread %d MSUs", dfg_thread->id);
            return -1;
        }
        log(LOG_DFG_INSTANTIATION, "Spawned %d MSUs on thread %d",
                   dfg_thread->n_msus, dfg_thread->id);
    }
    log(LOG_DFG_INSTANTIATION, "Initialized %d threads", n_threads);
    return 0;
}

int init_dfg_msu_types(struct dfg_msu_type **msu_types, int n_msu_types) {
    for (int i=0; i<n_msu_types; i++) {
        struct dfg_msu_type *type = msu_types[i];
        if (init_msu_type_id(type->id) != 0) {
            log_error("Could not instantiate required type %d", type->id);
            return -1;
        }
        log(LOG_DFG_INSTANTIATION, "Initialized MSU Type %d from dfg",
                   type->id);
    }
    log(LOG_DFG_INSTANTIATION, "Initialized %d MSU types", n_msu_types);
    return 0;
}

int instantiate_dfg_runtime(struct dfg_runtime *rt) {
    if (spawn_dfg_routes(rt->routes, rt->n_routes) != 0) {
        log_error("Error spawning routes for runtime DFG instantiation");
        return -1;
    }
    if (spawn_dfg_threads(rt->threads, rt->n_pinned_threads + rt->n_unpinned_threads) != 0) {
        log_error("Error spawning threads for runtime DFG instantiation");
        return -1;
    }
    if (add_all_dfg_route_endpoints(rt->routes, rt->n_routes) != 0) {
        log_error("Error adding endpoints to routes");
        return -1;
    }
    return 0;
}
