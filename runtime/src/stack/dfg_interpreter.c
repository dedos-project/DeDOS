#include "global_controller/dfg.h"
#include "control_protocol.h"
#include "runtime.h"
#include "routing.h"
#include "logging.h"

struct dedos_thread_msg *route_msg_from_vertices(struct dfg_vertex *from,
                                                 struct dfg_vertex *to){
    struct dedos_thread_msg *thread_msg = malloc(sizeof(*thread_msg));
    if (!thread_msg) {
        log_error("Could not allocate thread_msg for route creation");
        return NULL;
    }
    struct msu_control_add_route *add_route_msg = malloc(sizeof(*add_route_msg));
    if (!add_route_msg) {
        log_error("Could not allocate msg_control_add_route for route creation");
        free(thread_msg);
        return NULL;
    }

    thread_msg->action = MSU_ROUTE_ADD;
    thread_msg->action_data = from->msu_id;
    thread_msg->next = NULL;
    thread_msg->buffer_len = sizeof(*add_route_msg);
    thread_msg->data = add_route_msg;

    add_route_msg->peer_msu_id = to->msu_id;
    add_route_msg->peer_msu_type = to->msu_type;
    if ( from->scheduling->runtime->ip == to->scheduling->runtime->ip ){
        add_route_msg->peer_locality = 0;
        add_route_msg->peer_ipv4 = 0;
    } else {
        add_route_msg->peer_locality = 1;
        add_route_msg->peer_ipv4 = to->scheduling->runtime->ip;
    }

    return thread_msg;
}

int vertex_thread_id(struct dfg_vertex *vertex){
    return vertex->scheduling->thread_id;
}

int create_route_from_vertices(struct dfg_vertex *from, struct dfg_vertex *to){
    int thread_id = vertex_thread_id(from);
    if (thread_id < 0){
        log_error("Could not determine thread id for route %d->%d",
                  from->msu_id, to->msu_id);
        return -1;
    }
    struct dedos_thread_msg *msg = route_msg_from_vertices(from, to);
    if (msg == NULL){
        log_error("Could not create route %d->%d", from->msu_id, to->msu_id);
        return -1;
    }
    enqueue_msu_request(&all_threads[thread_id], msg);
    return 0;
}

int create_vertex_routes(struct dfg_vertex *vertex){
    struct dfg_edge_set *edge_set = vertex->scheduling->routing;
    int routes_created = 0;
    for (int i=0; i<edge_set->num_edges; i++) {
        if (create_route_from_vertices(vertex, edge_set->edges[i]) >= 0)
            routes_created++;
    }
    return routes_created;
}

struct runtime_endpoint *get_local_runtime(struct dfg_config *dfg, uint32_t local_ip) {
    printf("Checking %d runtimes\n", dfg->runtimes_cnt);
    for (int i=0; i<dfg->runtimes_cnt; i++) {
        printf("rt ip: %u\n", dfg->runtimes[i]->ip);
        if (dfg->runtimes[i]->ip == local_ip) {
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
    struct create_msu_thread_msg_data *create_action = malloc(sizeof(*create_action));
    if (!create_action) {
        log_error("Could not allocate thread_msg_data for MSU creation");
        free(thread_msg);
        return NULL;
    }
    thread_msg->action = CREATE_MSU;
    thread_msg->action_data = vertex->msu_id;
    thread_msg->data = create_action;
    thread_msg->next = NULL;

    create_action->msu_type = vertex->msu_type;
    //TODO(IMP): Initial data
    create_action->init_data_len = 0;
    create_action->creation_init_data = NULL;

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
    log_debug("Creation of MSU %d requested", vertex->msu_id);
    return 0;
}

int spawn_threads_from_dfg(struct dfg_config *dfg){
    int n_spawned_threads = 0;
    for (int i=0; i<dfg->vertex_cnt; i++){
        int thread_id = vertex_thread_id(dfg->vertices[i]);
        while (thread_id >= total_threads){
            n_spawned_threads++;
            int rtn = on_demand_create_worker_thread(0);
            if (rtn == 0){
                log_debug("Created worker thread to accomodate MSU");
            } else {
                log_error("Could not create necessary worker thread");
                return -1;
            }
        }
    }
    return n_spawned_threads;
}

int vertex_locality(struct dfg_vertex *vertex, uint32_t local_ip){
    if (vertex->scheduling->runtime->ip == local_ip)
        return 0;
    return 1;
}

int implement_dfg(struct dfg_config *dfg, uint32_t local_ip) {
    log_debug("Creating maximum of %d MSUs", dfg->vertex_cnt);
    int msus_created = 0;
    if ( spawn_threads_from_dfg < 0 ){
        log_error("Aborting DFG implementation");
        return -1;
    }
    for (int i=0; i<dfg->vertex_cnt; i++) {
        struct dfg_vertex *vertex = dfg->vertices[i];
        if (vertex_locality(vertex, local_ip) == 0) {
            if ( create_msu_from_vertex(vertex) < 0){
                log_debug("Failed to create msu %d", vertex->msu_id);
                continue;
            }
            msus_created++;
            int n_routes = create_vertex_routes(vertex);
            if ( n_routes < 0 ){
                log_error("Failed to create any routes for MSU %d", vertex->msu_id);
            } else {
                log_info("Created MSU %d, with %d outgoing routes",
                         vertex->msu_id, n_routes);
            }
        }
    }
    log_info("Created %d MSUs", msus_created);
    return 0;
}

