#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "logging.h"
#include "api.h"
#include "dfg.h"
#include "dfg_writer.h"
#include "control_protocol.h"
#include "communication.h"
#include "ip_utils.h"
#include "tmp_msu_headers.h"

#define SLEEP_AMOUNT_US 100000

/**
 * Add a new MSU to the DFG
 * @param msu_data: data to be used by the new MSU's init function
 * @param msu_id: ID for the new MSU
 * @param msu_type: type of the new MSU
 * @param msu_mode: blocking/non-blocking
 * @param thread_id: thread on which to place the new MSU
 * @param runtime_sock: target runtime
 */
int add_msu(char *msu_data, int msu_id, int msu_type,
            char *msu_mode, int thread_id, int runtime_sock) {

    struct dfg_config *dfg = NULL;
    struct dfg_vertex *new_msu = NULL;

    dfg = get_dfg();

    if (get_msu_from_id(msu_id) != NULL) {
        debug("An MSU with ID %d is already registered", msu_id);
        return -1;
    }

    int endpoint_index = -1;
    uint32_t num_pinned_threads;
    endpoint_index = get_sock_endpoint_index(runtime_sock);
    if (endpoint_index > -1) {
        pthread_mutex_lock(&dfg->dfg_mutex);

        new_msu = malloc(sizeof(struct dfg_vertex));
        if (new_msu == NULL) {
            debug("ERROR: %s", "could not allocate memory for new msu");
        }

        memcpy(new_msu->msu_mode, msu_mode, strlen(msu_mode));
        new_msu->msu_type = msu_type;
        new_msu->msu_id = msu_id;

        new_msu->scheduling.runtime = dfg->runtimes[endpoint_index];
        new_msu->scheduling.thread_id = (uint32_t)thread_id;

        num_pinned_threads = dfg->runtimes[endpoint_index]->num_pinned_threads;

        pthread_mutex_unlock(&dfg->dfg_mutex);

        if (num_pinned_threads == 0 || num_pinned_threads < thread_id) {
            debug("Destination runtime has %d worker threads. Can't fit new msu on thread %d",
                  num_pinned_threads, thread_id);
            free(new_msu);
            return -1;
        }
    } else {
        debug("ERROR: Couldn't find endpoint index for sock: %d", runtime_sock);
        return -1;
    }

    set_msu(new_msu);

    print_dfg();

    if (send_addmsu_msg(new_msu, msu_data) == -1) {
        debug("Could not send addmsu command to runtime");
        free(new_msu);
        return -1;
    } else {
        return 0;
    }
}


/**
 * Order the runtime to spawn a new MSU on a given thread
 * @param struct dfg_vertex: the new MSU object
 * @param char *data: initial data used by the MSU's init function
 * @return -1/0: failure/success
 */
//FIXME: assume msu creation goes well. We need some kind of acknowledgement from the runtime
int send_addmsu_msg(struct dfg_vertex *msu, char *init_data) {
    struct dedos_control_msg control_msg;
    size_t data_len;
    char data[MAX_INIT_DATA_LEN];
    memset(data, '\0', MAX_INIT_DATA_LEN);

    data_len = strlen(msu->msu_mode) + 1 + how_many_digits(msu->msu_id) + 1;
    snprintf(data, data_len + 1, "%s %d ", msu->msu_mode, msu->scheduling.thread_id);

    //TODO: create a template system for MSU such that we can query initial data.
    if (msu->msu_type == DEDOS_SOCKET_HANDLER_MSU_ID) {
        struct socket_handler_init_payload *ssl_init_data =
            malloc(sizeof(struct socket_handler_init_payload));

        ssl_init_data->port = 8080;
        ssl_init_data->domain = AF_INET;
        ssl_init_data->type = SOCK_STREAM;
        ssl_init_data->protocol = 0;
        ssl_init_data->bind_ip = INADDR_ANY;
        /*unsigned char buf[sizeof(struct in_addr)];
        ssl_inet_pton(AF_INET, "192.168.0.1", buf);
        ssl_init_data->bind_ip = buf;*/
        ssl_init_data->target_msu_type = 502; //DEDOS_SSL_REQUEST_ROUTING_MSU_ID;

        size_t len = sizeof(struct socket_handler_init_payload);
        memcpy(data + data_len, ssl_init_data, len);
        free(ssl_init_data);
        data_len += len;
    } else {
        if (init_data != NULL) {
            memcpy(data + data_len, init_data, strlen(init_data));
            data_len += strlen(init_data);
        }
    }

    char create_msu_msg_buffer[sizeof(struct manage_msu_control_payload) + data_len];

    struct manage_msu_control_payload *create_msu_msg =
        (struct manage_msu_control_payload*) create_msu_msg_buffer;
    create_msu_msg->msu_id         = msu->msu_id;
    create_msu_msg->msu_type       = msu->msu_type;
    create_msu_msg->init_data_size = data_len;
    create_msu_msg->init_data      = data;
    memcpy(create_msu_msg_buffer + sizeof(*create_msu_msg), data, data_len);

    debug("sock:%d, msu_type: %d, msu id: %d, init_data: %s, init_data_size: %d\n",
           msu->scheduling.runtime->sock,
           create_msu_msg->msu_type,
           create_msu_msg->msu_id,
           create_msu_msg->init_data,
           create_msu_msg->init_data_size);

    control_msg.msg_type = ACTION_MESSAGE;
    control_msg.msg_code = CREATE_MSU;
    control_msg.header_len = sizeof(struct dedos_control_msg); //might be redundant
    control_msg.payload_len =
        sizeof(struct manage_msu_control_payload) + create_msu_msg->init_data_size;
    control_msg.payload = create_msu_msg_buffer;
    send_control_msg(msu->scheduling.runtime->sock, &control_msg);

    usleep(SLEEP_AMOUNT_US);

    return 0;
}

int unwire_msu(int msu_id) {
    struct dfg_config *dfg = get_dfg();
    struct dfg_vertex *msu = get_msu_from_id(msu_id);

    for (int rt_i = 0; rt_i < dfg->runtimes_cnt; rt_i++) {
        struct dfg_runtime_endpoint *rt = dfg->runtimes[rt_i];

        for (int route_i = 0 ; route_i < rt->num_routes; route_i++) {
            struct dfg_route *route = rt->routes[route_i];

            for (int msu_i = 0; msu_i < route->num_destinations; msu_i++) {
                struct dfg_vertex *msu = route->destinations[msu_i];

                if (msu->msu_id == msu_id) {
                    int rtn;
                    int runtime_index = get_sock_endpoint_index(rt->sock);

                    rtn = dfg_del_route_endpoint(runtime_index, route->route_id, msu_id);
                    if (rtn != 0) {
                        log_error("Could not remove endpoint %d from route %d in DFG",
                                   msu_id, route->route_id);
                        return -1;
                    }

                    rtn = del_endpoint(msu_id, route->route_id, rt->sock);
                    if (rtn != 0) {
                        log_error("Could not send del_endpoint order to runtime %d");
                        return -1;
                    }

                    log_debug("Removed msu %d from route %d", msu_id, route->route_id);
                }
            }
        }
    }

    return 0;
}

/**
 * Remove an MSU from the system.
 * First unwire the MSU, then remove it from the DFG, and forward this order to the host runtime
 * @param msu_id: ID of the MSU to be removed
 * @param msu_type: type of the MSU to be removed
 * @param runtime_sock: target runtime //FIXME: this is useless.
 * @return: -1/0: failure/success
 */
int del_msu(int msu_id, int msu_type) {
    struct dfg_vertex *msu = get_msu_from_id(msu_id);
    int runtime_sock = msu->scheduling.runtime->sock;

    if (msu == NULL) {
        log_error("Cannot delete non-existent MSU %d", msu_id);
        return -1;
    }

    unwire_msu(msu_id);
    remove_msu_from_dfg(msu_id);

    struct manage_msu_control_payload delete_msg = {
        .msu_id = msu_id,
        .msu_type = (unsigned int) msu_type,
        .init_data_size = 0,
        .init_data = NULL
    };

    log_debug("\nDelete MSU msg:\n Sock: %d\n msu_type: %d\n msu_id: %d",
              runtime_sock, msu_type, msu_id);

    struct dedos_control_msg control_msg = {
        .msg_type = ACTION_MESSAGE,
        .msg_code = DESTROY_MSU,
        .header_len = sizeof(control_msg),
        .payload_len = sizeof(delete_msg),
        .payload = &delete_msg
    };

    return send_control_msg(runtime_sock, &control_msg);
}

/**
 * Attach a route to an MSU and instruct a runtime to do so
 * @param msu_id: ID of target MSU
 * @param route_id: ID of target route
 * @param runtime_sock: listening socket for target runtime
 * @return -1/0: failure/success
 */
int add_route(int msu_id, int route_id, int runtime_sock){
    // First, update the DFG
    // (this shows that the operation should be valid, and is a stop-gap measure until
    // acknowledgement from the runtime is implemented)

    int rt_index = get_sock_endpoint_index(runtime_sock);
    int rtn = add_route_to_msu_vertex(rt_index, msu_id, route_id);

    if (rtn < 0) {
        log_error("Error adding route to vertex! Not proceeding!");
        return -1;
    }

    rtn = send_addroute_msg(route_id, msu_id, runtime_sock);
    return rtn;
}

/**
 * Instruct a runtime to do attach a route to an MSU
 * @param msu_id: ID of target MSU
 * @param route_id: ID of target route
 * @param runtime_sock: listening socket for target runtime
 * @return -1/0: failure/success
 */
int send_addroute_msg(int route_id, int msu_id, int runtime_sock) {
    struct dfg_vertex *msu = get_msu_from_id(msu_id);
    int rtn;

    struct manage_msu_control_payload add_route_msg = {
        .msu_id = msu_id,
        .msu_type = (unsigned int) msu->msu_type,
        .route_id = route_id,
        .init_data_size = 0,
        .init_data = NULL
    };

    debug("sock:%d, msu_type: %d, msu id: %d, route_id: %d, init_data: %s, init_data_size: %d\n",
           msu->scheduling.runtime->sock,
           add_route_msg.msu_type,
           add_route_msg.msu_id,
           add_route_msg.route_id,
           add_route_msg.init_data,
           add_route_msg.init_data_size);

    struct dedos_control_msg control_msg = {
        .msg_type = ACTION_MESSAGE,
        .msg_code = ADD_ROUTE_TO_MSU,
        .header_len = sizeof(control_msg),
        .payload_len = sizeof(add_route_msg),
        .payload = &add_route_msg
    };

    rtn = send_control_msg(runtime_sock, &control_msg);
    usleep(SLEEP_AMOUNT_US);
    return rtn;
}

int del_route(int msu_id, int route_id, int runtime_sock){
    // First, update the DFG
    // (this shows that the operation should be valid, and is a stop-gap measure until
    // acknowledgement from the runtime is implemented)

    int rt_index = get_sock_endpoint_index(runtime_sock);
    int rtn = del_route_from_msu_vertex(rt_index, msu_id, route_id);

    if ( rtn < 0 ) {
        log_error("Error deleting route from vertex! Not proceeding!");
        return -1;
    }

    struct dfg_vertex *msu = get_msu_from_id(msu_id);

    struct manage_msu_control_payload del_route_msg = {
        .msu_id = msu_id,
        .msu_type = (unsigned int) msu->msu_type,
        .route_id = route_id,
        .init_data_size = 0,
        .init_data = NULL
    };

    struct dedos_control_msg control_msg = {
        .msg_type = ACTION_MESSAGE,
        .msg_code = ADD_ROUTE_TO_MSU,
        .header_len = sizeof(control_msg),
        .payload_len = sizeof(del_route_msg),
        .payload = &del_route_msg
    };

    rtn =send_control_msg(runtime_sock, &control_msg);
    usleep(SLEEP_AMOUNT_US);
    return rtn;
}

/**
 * Attach an endpoint to a route and instruct a runtime to do so
 * @param msu_id: ID of target MSU
 * @param route_id: ID of target route
 * @param runtime_sock: listening socket for target runtime
 * @return -1/0: failure/success
 */
int add_endpoint(int msu_id, int route_num, unsigned int range_end, int runtime_sock){
    int rt_index = get_sock_endpoint_index(runtime_sock);
    int rtn = dfg_add_route_endpoint(rt_index, route_num, msu_id, range_end);

    if (rtn < 0 ){
        log_error("Error adding route endpoint! Not proceeding!");
        return -1;
    }

    return send_addendpoint_msg(msu_id, rt_index, route_num, range_end, runtime_sock);
}

/**
 * Instruct a runtime to add an endpoint to a route
 * @param msu_id: ID of target MSU
 * @param route_id: ID of target route
 * @param runtime_sock: listening socket for target runtime
 * @return -1/0: failure/success
 */
int send_addendpoint_msg(int msu_id, int rt_index, int route_num,
                         unsigned int range_end, int runtime_sock) {
    struct dfg_vertex *msu = get_msu_from_id(msu_id);
    int rtn;

    enum locality locality;
    uint32_t ip = 0;
    if (get_sock_endpoint_index(msu->scheduling.runtime->sock) != rt_index) {
        locality = MSU_IS_REMOTE;
        ip = msu->scheduling.runtime->ip;
    } else {
        locality = MSU_IS_LOCAL;
    }

    struct manage_route_control_payload route_msg = {
        .msu_id = msu_id,
        .msu_type_id = msu->msu_type,
        .route_id = route_num,
        .key_range = range_end,
        .locality = locality,
        .ipv4 = ip
    };

    debug("sock:%d, msu_type: %d, msu id: %d, route_id: %d, key_range: %d, loc: %d, ipv4: %d\n",
           runtime_sock,
           route_msg.msu_type_id,
           route_msg.msu_id,
           route_msg.route_id,
           route_msg.key_range,
           route_msg.locality,
           route_msg.ipv4);

    struct dedos_control_msg control_msg = {
        .msg_type = ACTION_MESSAGE,
        .msg_code = ROUTE_ADD_ENDPOINT,
        .header_len = sizeof(control_msg),
        .payload_len = sizeof(route_msg),
        .payload = &route_msg
    };

    rtn = send_control_msg(runtime_sock, &control_msg);
    usleep(SLEEP_AMOUNT_US);
    return rtn;
}

int del_endpoint(int msu_id, int route_id, int runtime_sock) {
    int rt_index = get_sock_endpoint_index(runtime_sock);
    int rtn = dfg_del_route_endpoint(rt_index, route_id, msu_id);

    if (rtn < 0) {
        log_error("Error deleting route endpoint! Not proceeding!");
        return -1;
    }

    struct dfg_vertex *msu = get_msu_from_id(msu_id);

    struct manage_route_control_payload route_msg = {
        .msu_id = msu_id,
        .msu_type_id = msu->msu_type,
        .route_id = route_id,
    };

    struct dedos_control_msg control_msg = {
        .msg_type = ACTION_MESSAGE,
        .msg_code = ROUTE_DEL_ENDPOINT,
        .header_len = sizeof(control_msg),
        .payload_len = sizeof(route_msg),
        .payload = &route_msg
    };

    rtn = send_control_msg(runtime_sock, &control_msg);
    usleep(SLEEP_AMOUNT_US);
    return rtn;
}

int mod_endpoint(int msu_id, int route_id, unsigned int range_end, int runtime_sock) {
    // TODO: This is inefficient! It will do for now...
    int rt_index = get_sock_endpoint_index(runtime_sock);
    int rtn = dfg_del_route_endpoint(rt_index, route_id, msu_id);
    if (rtn < 0){
        log_error("Error removing route endpoint for modification! Not proceeding!");
        return -1;
    }
    rtn = dfg_add_route_endpoint(rt_index, route_id, msu_id, range_end);
    if (rtn < 0){
        log_error("Error re-adding route endpoint for modification! Not proceeding!");
        return -1;
    }

    struct dfg_vertex *msu = get_msu_from_id(msu_id);

    struct manage_route_control_payload route_msg = {
        .msu_id = msu_id,
        .msu_type_id = msu->msu_type,
        .route_id = route_id,
        .key_range = range_end
    };

    struct dedos_control_msg control_msg = {
        .msg_type = ACTION_MESSAGE,
        .msg_code = ROUTE_MOD_ENDPOINT,
        .header_len = sizeof(control_msg),
        .payload_len = sizeof(route_msg),
        .payload = &route_msg
    };

    rtn = send_control_msg(runtime_sock, &control_msg);
    usleep(SLEEP_AMOUNT_US);
    return rtn;
}

int create_worker_thread(int runtime_sock) {
    struct dedos_control_msg control_msg;
    uint32_t num_pinned_threads, num_cores;
    int ret;
    struct dfg_config *dfg;
    struct dfg_runtime_endpoint *rt;

    dfg = get_dfg();

    int endpoint_index = -1;
    endpoint_index = get_sock_endpoint_index(runtime_sock);
    if (endpoint_index > -1) {
        pthread_mutex_lock(&dfg->dfg_mutex);

        rt = dfg->runtimes[endpoint_index];
        num_pinned_threads = rt->num_pinned_threads;
        num_cores = rt->num_cores;

        pthread_mutex_unlock(&dfg->dfg_mutex);

        if (num_cores == (num_pinned_threads + 1)) {
            debug("Destination runtime is maxed out. Cores: %u, Pinned threads: %u",
                    num_cores, num_pinned_threads);
            return -1;
        }
    } else {
        debug("Couldn't find endpoint index for sock: %d", runtime_sock);
        return -1;
    }

    control_msg.msg_type = ACTION_MESSAGE;
    control_msg.msg_code = CREATE_PINNED_THREAD;
    control_msg.payload_len = 0;
    control_msg.header_len = sizeof(struct dedos_control_msg);

    ret = send_control_msg(runtime_sock, &control_msg);

    if (ret == 0) {
        pthread_mutex_lock(&dfg->dfg_mutex);

        struct runtime_thread *new_thread = NULL;
        new_thread = malloc(sizeof(struct runtime_thread));
        if (new_thread == NULL) {
            debug("Could not allocate memory for runtime thread structure");
            return -1;
        }

        new_thread->id = rt->num_threads;
        new_thread->mode = 1;
        new_thread->utilization = 0;
        new_thread->num_msus = 0;

        int new_thread_index = rt->num_threads;
        rt->threads[new_thread_index] = new_thread;
        rt->num_pinned_threads++;
        rt->num_threads++;

        pthread_mutex_unlock(&dfg->dfg_mutex);
    } else {
        debug("Failed to send create worker thread command to runtime");
        return ret;
    }

    return ret;

}

int add_runtime(char *runtime_ip, int runtime_sock) {
    // prepare data to update dfg
    struct dedos_dfg_manage_msg *dfg_manage_msg = malloc(sizeof(struct dedos_dfg_manage_msg));
    dfg_manage_msg->msg_type = ACTION_MESSAGE;
    dfg_manage_msg->msg_code = RUNTIME_ENDPOINT_ADD;
    dfg_manage_msg->header_len = sizeof(struct dedos_dfg_manage_msg);
    dfg_manage_msg->payload_len = sizeof(struct dedos_dfg_add_endpoint_msg *);

    struct dedos_dfg_add_endpoint_msg *endpoint_msg =
        malloc(sizeof(struct dedos_dfg_add_endpoint_msg));

    endpoint_msg->runtime_sock = runtime_sock;


    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    if (strcmp(runtime_ip, "127.0.0.1") == 0) {
        //maybe that lock is useless as ctl's ip is not expected to change
        pthread_mutex_lock(&dfg->dfg_mutex);
        strncpy(endpoint_msg->runtime_ip, dfg->global_ctl_ip, INET_ADDRSTRLEN);
        pthread_mutex_unlock(&dfg->dfg_mutex);

    } else {
        strncpy(endpoint_msg->runtime_ip, runtime_ip, INET_ADDRSTRLEN);
    }

    dfg_manage_msg->payload = endpoint_msg;

    update_dfg(dfg_manage_msg);
    print_dfg();

    return 0;
}

/**
 * Display an MSU stats on stdout. This is a temporary solution before returning a data structure
 * exploitatable by anyone calling this API function
 * @param msu_id
 * @return none
 */
void show_stats(int msu_id) {
    struct dfg_vertex *msu = get_msu_from_id(msu_id);
    int i;

    printf("queue_item_processed\n");
    printf("value, timestamp\n");
    for (i = 0; i < RRDB_ENTRIES; ++i) {
        if (msu->statistics.queue_items_processed.time[i].tv_sec > 0) {
            printf("%d, %d\n",
                    msu->statistics.queue_items_processed.data[i],
                    msu->statistics.queue_items_processed.time[i]);
        }
    }

    printf("data_queue_size\n");
    printf("value, timestamp\n");
    for (i = 0; i < RRDB_ENTRIES; ++i) {
        if (msu->statistics.queue_length.time[i].tv_sec > 0) {
            printf("%d, %d\n",
                    msu->statistics.queue_length.data[i],
                    msu->statistics.queue_length.time[i]);
        }
    }

    printf("memory_allocated\n");
    printf("value, timestamp\n");
    for (i = 0; i < RRDB_ENTRIES; ++i) {
        if (msu->statistics.memory_allocated.time[i].tv_sec > 0) {
            printf("%d, %d\n",
                    msu->statistics.memory_allocated.data[i],
                    msu->statistics.memory_allocated.time[i]);
        }
    }
}
