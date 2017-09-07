#include "logging.h"
#include "local_msu.h"
#include "dfg.h"
#include "routing.h"

static int add_dfg_route_destinations(int route_id,
                                      struct dfg_route_destination **destinations,
                                      int n_destinations) {
    for (int i=0; i<n_destinations; i++) {
        struct msu_endpoint endpoint;
        struct dfg_route_destination *dest = destinations[i];
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
    log_custom(LOG_DFG_INSTANTIATION, "Added %d destinations to route %d",
               n_destinations, route_id);
    return 0;
}

static int spawn_dfg_routes(struct dfg_route **routes, int n_routes) {
    for (int i=0; i<n_routes; i++) {
        struct dfg_route *dfg_route = routes[i];
        if (init_route(dfg_route->id, dfg_route->msu_type->id) != 0) {
            log_error("Could not instantiate route %d (type: %d)",
                      dfg_route->id, dfg_route->msu_type->id);
            return -1;
        }
        if (add_dfg_route_destinations(dfg_route->id,
                                       dfg_route->destinations,
                                       dfg_route->n_destinations) != 0) {
            log_error("Error adding destinations to route %d", dfg_route->id);
            return -1;
        }
    }
    log_custom(LOG_DFG_INSTANTIATION, "Spawned %d routes", n_routes);
    return 0;
}

static int add_dfg_routes_to_msu(struct local_msu *msu, struct dfg_route **routes, int n_routes) {
    for (int i=0; i<n_routes; i++) {
        struct dfg_route *route = routes[i];
        if (add_route_to_set(&msu->routes,  route->id) != 0) {
            log_error("Error adding route %d to msu %d", route->id, msu->id);
            return -1;
        }
    }
    log_custom(LOG_DFG_INSTANTIATION, "Added %d routes to msu %d",
               n_routes, msu->id);
    return 0;
}

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
        log_custom(LOG_DFG_INSTANTIATION, "Initialized MSU %d from dfg", dfg_msu->id);
        struct dfg_scheduling *sched = &dfg_msu->scheduling;
        if (add_dfg_routes_to_msu(msu, sched->routes, sched->n_routes) != 0) {
            log_error("Error adding routes to MSU %d", msu->id);
            return -1;
        }
    }
    log_custom(LOG_DFG_INSTANTIATION, "Initialized %d MSUs", n_msus);
    return 0;
}

static int spawn_dfg_threads(struct dfg_thread **threads, int n_threads) {
    for (int i=0; i<n_threads; i++) {
        struct dfg_thread *dfg_thread = threads[i];
        struct worker_thread *thread = create_worker_thread(dfg_thread->id, dfg_thread->mode);
        if (thread == NULL) {
            log_error("Error instantiating thread %d! Can not continue!", dfg_thread->id);
            return -1;
        }
        log_custom(LOG_DFG_INSTANTIATION, "Created worker thread %d, mode = %s",
                   dfg_thread->id, dfg_thread->mode == PINNED_THREAD ? "Pinned" : "Unpinned");
        if (spawn_dfg_msus(thread, dfg_thread->msus, dfg_thread->n_msus) != 0) {
            log_error("Error instantiating thread %d MSUs", dfg_thread->id);
            return -1;
        }
    }
    log_custom(LOG_DFG_INSTANTIATION, "Initialized %d threads", n_threads);
    return 0;
}

int init_dfg_msu_types(struct dfg_msu_type **msu_types, int n_msu_types) {
    for (int i=0; i<n_msu_types; i++) {
        struct dfg_msu_type *type = msu_types[i];
        if (init_msu_type_id(type->id) != 0) {
            log_error("Could not instantiate required type %d", type->id);
            return -1;
        }
        log_custom(LOG_DFG_INSTANTIATION, "Initialized MSU Type %d from dfg",
                   type->id);
    }
    log_custom(LOG_DFG_INSTANTIATION, "Initialized %d MSU types", n_msu_types);
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
    return 0;
}
