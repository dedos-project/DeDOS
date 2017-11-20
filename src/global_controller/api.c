#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>


#include "controller_stats.h"
#include "controller_mysql.h"
#include "logging.h"
#include "api.h"
#include "dfg.h"
#include "communication.h"
#include "msu_ids.h"
#include "controller_dfg.h"
#include "runtime_messages.h"

int add_msu(unsigned int msu_id, unsigned int type_id,
            char *init_data_c, char *msu_mode, char *vertex_type_c,
            unsigned int thread_id, unsigned int runtime_id) {

    struct dfg_msu_type *type = get_dfg_msu_type(type_id);
    if (type == NULL) {
        log_error("Could not get MSU type %d", type_id);
        return -1;
    }

    enum blocking_mode mode = str_to_blocking_mode(msu_mode);
    if (mode == UNKNOWN_BLOCKING_MODE) {
        log_error("Could not get blocking mode from %s", msu_mode);
        return -1;
    }

    uint8_t vertex_type = str_to_vertex_type(vertex_type_c);

    struct msu_init_data init_data;
    strcpy(init_data.init_data, init_data_c);

    struct dfg_msu *msu = init_dfg_msu(msu_id, type, vertex_type, mode, &init_data);

    if (msu == NULL) {
        log_error("Error innitializing dfg MSU %d", msu_id);
        return -1;
    }

    int rtn = schedule_dfg_msu(msu, runtime_id, thread_id);
    if (rtn < 0) {
        log_error("Error scheduling MSU on DFG");
        return -1;
    }

    if (send_create_msu_msg(msu) == -1) {
        log_error("Could not send create-msu-msg to runtime %u", runtime_id);
        unschedule_dfg_msu(msu);
        return -1;
    } else {
        register_msu_stats(msu_id, thread_id, runtime_id);
        return 0;
    }
}

int remove_msu(unsigned int id) {
    struct dfg_msu *msu = get_dfg_msu(id);

    if (msu == NULL) {
        log_error("MSU %u not scheduled!", id);
        return -1;
    }

    int rtn = send_delete_msu_msg(msu);

    if (rtn < 0) {
        log_error("Error sending delete MSU msg for MSU %d", id);
        return -1;
    }

    rtn = unschedule_dfg_msu(msu);
    if (rtn < 0) {
        log_error("Error unscheduling DFG msu");
        return -1;
    }
    unregister_msu_stats(id);

    return 0;
}

int create_route(unsigned int route_id, unsigned int type_id, unsigned int runtime_id) {
    struct dfg_msu_type *type = get_dfg_msu_type(type_id);
    if (type == NULL) {
        log_error("Could not find MSU type %u in dfg", type_id);
        return -1;
    }

    struct dfg_route *route = create_dfg_route(route_id, type, runtime_id);
    if (route == NULL) {
        log_error("Could not create route %u in dfg", route_id);
        return -1;
    }

    int rtn = send_create_route_msg(route);
    if (rtn < 0) {
        log_error("Error sending create route message");
        // TODO: Delete route
        return -1;
    }
    return 0;
}

int delete_route(unsigned int route_id) {
    struct dfg_route *route = get_dfg_route(route_id);
    if (route == NULL) {
        log_error("Route %u does not exist", route_id);
        return -1;
    }
    int rtn = send_delete_route_msg(route);
    if (rtn < 0) {
        log_error("Error sending delete route message");
        return -1;
    }

    rtn = delete_dfg_route(route);
    if (rtn < 0) {
        log_error("Error deleting DFG route");
        return -1;
    }

    return 0;
}

int add_route_to_msu(unsigned int msu_id, unsigned int route_id) {
    struct dfg_route *route = get_dfg_route(route_id);
    if (route == NULL) {
        log_error("Could not retrieve route %u from dfg", route_id);
        return -1;
    }
    struct dfg_msu *msu = get_dfg_msu(msu_id);
    if (msu == NULL) {
        log_error("Could not retrieve MSU %u from DFG", msu_id);
        return -1;
    }

    int rtn = add_dfg_route_to_msu(route, msu);

    if (rtn < 0) {
        log_error("Error adding route to MSU");
        return -1;
    }

    rtn = send_add_route_to_msu_msg(route, msu);
    if (rtn < 0) {
        log_error("Error sending add route to MSU msg");
        // TODO: Remove route from MSU
        return -1;
    }
    return 0;
}

// TODO: del_route_from_msu()

int add_endpoint(unsigned int msu_id, uint32_t key, unsigned int route_id) {
    struct dfg_route *route = get_dfg_route(route_id);
    if (route == NULL) {
        log_error("Could not retrieve route %u from dfg", route_id);
        return -1;
    }
    struct dfg_msu *msu = get_dfg_msu(msu_id);
    if (msu == NULL) {
        log_error("Could not retrieve MSU %u from DFG", msu_id);
        return -1;
    }

    struct dfg_route_endpoint *endpoint = add_dfg_route_endpoint(msu, key, route);
    if (endpoint == NULL) {
        log_error("Error adding MSU to route");
        return -1;
    }

    int rtn = send_add_endpoint_msg(route, endpoint);
    if (rtn < 0) {
        log_error("Error sending add endpoint message");
        // TODO: Remove endpoint
        return -1;
    }
    log(LOG_API_CALLS, "Added endpoint %u with key %d to route %u",
        msu_id, (int)key, route_id);
    return 0;
}

int del_endpoint(unsigned int msu_id, unsigned int route_id) {
    struct dfg_route *route = get_dfg_route(route_id);
    if (route == NULL) {
        log_error("Could not retrieve route %u from dfg", route_id);
        return -1;
    }

    struct dfg_route_endpoint *ep = get_dfg_route_endpoint(route, msu_id);
    if (ep == NULL) {
        log_error("Could not get endpoint %u from dfg route", msu_id);
        return -1;
    }

    int rtn = send_del_endpoint_msg(route, ep);
    if (rtn < 0) {
        log_error("Error sending delete endpoint message");
        return -1;
    }

    rtn = del_dfg_route_endpoint(route, ep);
    if (rtn < 0) {
        log_error("Error deleting endpoint from DFG route");
        return -1;
    }

    return 0;
}

int mod_endpoint(unsigned int msu_id, uint32_t key, unsigned int route_id) {
    struct dfg_route *route = get_dfg_route(route_id);
    if (route == NULL) {
        log_error("Could not retrieve route %u from dfg", route_id);
        return -1;
    }

    struct dfg_route_endpoint *ep = get_dfg_route_endpoint(route, msu_id);
    if (ep == NULL) {
        log_error("Could not get endpoint %u from dfg route", msu_id);
        return -1;
    }

    int rtn = mod_dfg_route_endpoint(route, ep, key);
    if (rtn < 0) {
        log_error("Could not modify DFG route endpoint");
        return -1;
    }

    rtn = send_mod_endpoint_msg(route, ep);
    if (rtn < 0) {
        log_error("Error sending message to modify route endpoint");
        return -1;
    }
    return 0;
}

int create_worker_thread(unsigned int thread_id, unsigned int runtime_id, char *mode) {
    struct dfg_runtime *rt = get_dfg_runtime(runtime_id);
    if (rt == NULL) {
        log_error("could not retrieve runtime %u from dfg", runtime_id);
        return -1;
    }

    struct dfg_thread *thread = get_dfg_thread(rt, thread_id);
    if (thread != NULL) {
        log_error("Thread %u already exists on runtime %u", thread_id, runtime_id);
        return -1;
    }

    enum thread_mode mode_e = str_to_thread_mode(mode);
    if (mode_e == 0) {
        log_error("Could not get mode from %s", mode);
        return -1;
    }

    thread = create_dfg_thread(rt, thread_id, mode_e);
    if (thread == NULL) {
        log_error("Error creating dfg thread");
        return -1;
    };

    int rtn = send_create_thread_msg(thread, rt);
    if (rtn < 0) {
        log_error("Error sending create_thread_msg");
        return -1;
    }
    register_thread_stats(thread_id, runtime_id);
    return 0;
}

