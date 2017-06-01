#include "communication.h"
#include "dfg_interpreter.h"
#include "dfg.h"
#include "control_protocol.h"
#include "msu_tracker.h"
#include "control_operations.h"
#include "runtime.h"
#include "routing.h"
#include "logging.h"


int vertex_thread_id(struct dfg_vertex *vertex){
    return vertex->scheduling.thread_id;
}

int add_route_to_msu(struct dfg_vertex *vertex, struct dfg_route *route){
    int thread_id = vertex_thread_id(vertex);
    if (thread_id < 0){
        log_error("Could not determine thread id for msu %d",
                  vertex->msu_id);
        return -1;
    }

    struct msu_placement_tracker *tracker= msu_tracker_find(vertex->msu_id);
    if (!tracker){
        log_error("Couldn't find msu tracker for MSU %d", vertex->msu_id);
        return -1;
    }

    struct generic_msu *msu = dedos_msu_pool_find(tracker->dedos_thread->msu_pool,
                                                  vertex->msu_id);
    if (!msu){
        log_error("Failed to get ptr to  MSU %d from msu pool", vertex->msu_id);
        return -1;
    }

    int rtn = add_route_to_set(&msu->routes, route->route_id);

    if (rtn < 0){
        log_error("Could not add route %d to msu %d", route->route_id, vertex->msu_id);
        return -1;
    }
    return 0;
}

int add_all_routes_to_msu(struct dfg_vertex *vertex){
    int routes_created = 0;
    for (int i=0; i<vertex->scheduling.num_routes; i++) {
        if (add_route_to_msu(vertex, vertex->scheduling.routes[i]) >= 0)
            routes_created++;
    }
    return routes_created;
}

struct dfg_runtime_endpoint *get_local_runtime(struct dfg_config *dfg, int runtime_id) {
    printf("Checking %d runtimes\n", dfg->runtimes_cnt);
    for (int i=0; i<dfg->runtimes_cnt; i++) {
        if (dfg->runtimes[i]->id == runtime_id) {
            return dfg->runtimes[i];
        }
    }
    return NULL;
}

struct dedos_thread_msg *msu_msg_from_vertex(struct dfg_vertex *vertex){
    struct dedos_thread_msg *thread_msg = malloc(sizeof(*thread_msg));
    if (!thread_msg) {
        log_error("Could not allocate thread_msg for MSU creation");
        return NULL;
    }
    struct create_msu_thread_data *create_action = malloc(sizeof(*create_action));
    if (!create_action) {
        log_error("Could not allocate thread_data for MSU creation");
        free(thread_msg);
        return NULL;
    }
    thread_msg->action = CREATE_MSU;
    thread_msg->action_data = vertex->msu_id;
    thread_msg->data = create_action;
    thread_msg->next = NULL;

    create_action->msu_type = vertex->msu_type;
    create_action->msu_id = vertex->msu_id;
    //TODO(IMP): Initial data
    create_action->init_data_len = 0;
    create_action->init_data = NULL;

    //Size of initial data will have to be added to this value
    thread_msg->buffer_len = sizeof(*create_action);

    return thread_msg;
}

int create_msu_from_vertex(struct dfg_vertex *vertex){
    int thread_id = vertex_thread_id(vertex);
    if (thread_id > total_threads){
        log_error("Cannot create MSU on nonexistent thread %d", thread_id);
        return -1;
    }
    struct dedos_thread_msg *msg = msu_msg_from_vertex(vertex);
    if (!msg)
        return -1;
    log_debug("Requesting MSU creation");
    create_msu_request(&all_threads[thread_id], msg);
    // Store the placement info in msu_placements hash structure,
    // though we don't know yet if the creation will succeed.
    // If the creation fails the thread creating the MSU should
    // enqueue a FAIL_CREATE_MSU msg.
    log_debug("Tracking MSU %d", vertex->msu_id);
    msu_tracker_add(vertex->msu_id, &all_threads[thread_id]);
    log_debug("Creation of MSU %d (type %d) requested", vertex->msu_id, vertex->msu_type);
    return 0;
}

int vertex_locality(struct dfg_vertex *vertex, int runtime_id){
    if (vertex->scheduling.runtime->id == runtime_id)
        return 0;
    return 1;
}
int add_msus_to_route(struct dfg_route *dfg_route, int runtime_id){
    int errored = 0;
    for (int i=0; i<dfg_route->num_destinations; i++){
        struct dfg_vertex *vertex = dfg_route->destinations[i];
        struct msu_endpoint endpoint;
        endpoint.type_id = vertex->msu_type;
        endpoint.id = vertex->msu_id;
        if ( vertex_locality(vertex, runtime_id) == 0){
            endpoint.locality = MSU_IS_LOCAL;
            endpoint.msu_queue = get_msu_queue(endpoint.id, endpoint.type_id);
            endpoint.ipv4 = 0;
        } else {
            endpoint.locality = MSU_IS_REMOTE;
            endpoint.ipv4 = vertex->scheduling.runtime->ip;
            endpoint.msu_queue = NULL;
        }
        int rtn = add_route_endpoint(dfg_route->route_id,
                                     &endpoint,
                                     dfg_route->destination_keys[i]);
        if (rtn < 0){
            errored = -1;
            log_error("Could not add msu %d to route %d", endpoint.id, dfg_route->route_id);
        }
    }
    return errored;
}


int create_route_from_dfg(struct dfg_route *dfg_route){
    if (init_route(dfg_route->route_id, dfg_route->msu_type) != 0){
        log_error("Error initializing route %d from dfg",
                   dfg_route->route_id);
        return -1;
    }
    return 0;
}

int spawn_threads_from_dfg(struct dfg_config *dfg, int runtime_id){
    int n_spawned_threads = 0;
    for (int i=0; i<dfg->vertex_cnt; i++){
        if (vertex_locality(dfg->vertices[i], runtime_id) == 0) {
            int thread_id = vertex_thread_id(dfg->vertices[i]);
            while (thread_id >= total_threads){
                n_spawned_threads++;
                int rtn = on_demand_create_worker_thread(0);
                if (rtn >= 0){
                    log_debug("Created worker thread to accomodate MSU");
                } else {
                    log_error("Could not create necessary worker thread");
                    return -1;
                }
            }
        }
    }
    return n_spawned_threads;
}


int implement_dfg(struct dfg_config *dfg, int runtime_id) {
    log_debug("Creating maximum of %d MSUs", dfg->vertex_cnt);
    int msus_created = 0;
    if ( spawn_threads_from_dfg(dfg, runtime_id) < 0 ){
        log_error("Aborting DFG implementation");
        return -1;
    }

    // First, create all routes
    struct dfg_runtime_endpoint *runtime = get_local_runtime(dfg, runtime_id);
    for (int i=0; i<runtime->num_routes; i++){
        if (create_route_from_dfg(runtime->routes[i]) != 0){
            log_debug("failed to create route %d", runtime->routes[i]->route_id);
        }
    }


    // Loop through once to create the MSUs
    for (int i=0; i<dfg->vertex_cnt; i++) {
        struct dfg_vertex *vertex = dfg->vertices[i];
        if (vertex_locality(vertex, runtime_id) == 0) {
            if ( create_msu_from_vertex(vertex) < 0){
                log_debug("Failed to create msu %d", vertex->msu_id);
                continue;
            }
            log_info("Created MSU %d", vertex->msu_id);
            msus_created++;
        }
    }
    // Make sure the MSU creation goes through
    int ret = check_comm_sockets();
    if (ret < 0){
        log_warn("check_comm_sockets failed");
    }
    sleep(2);

    // Now add the routes to the MSUs
    for (int i=0; i<dfg->vertex_cnt; i++){
        struct dfg_vertex *vertex = dfg->vertices[i];
        if (vertex_locality(vertex, runtime_id) == 0) {
            int n_routes = add_all_routes_to_msu(vertex);
            if ( n_routes < 0 ){
                log_error("Failed to create any routes for MSU %d", vertex->msu_id);
            } else {
                log_info("Created %d outgoing routes for MSU %d",
                         n_routes, vertex->msu_id);
            }
        }
    }

    // Now add the MSUs to the routes
    for (int i=0; i<runtime->num_routes; i++){
        if (add_msus_to_route(runtime->routes[i], runtime_id) != 0){
            log_debug("Failed to add MSUs to route %d", runtime->routes[i]->route_id);
        }
    }
    log_info("Created %d MSUs", msus_created);
    return 0;
}

