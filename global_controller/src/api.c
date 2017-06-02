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

int add_msu(char *msu_data, int msu_id, int msu_type,
            char *msu_mode, int thread_id, int runtime_sock) {
    char *buf;
    long total_msg_size = 0;
    struct dedos_control_msg control_msg;
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
            debug("ERROR: Destination runtime has %d worker threads. Can't fit new msu on thread %d",
                  num_pinned_threads, thread_id);
            free(new_msu);
            return -1;
        }
    } else {
        debug("ERROR: Couldn't find endpoint index for sock: %d", runtime_sock);
        return -1;
    }

    // [mode][space][thread_id][\0]
    size_t data_len;
    char data[MAX_INIT_DATA_LEN];
    memset(data, '\0', MAX_INIT_DATA_LEN);

    data_len = strlen(msu_mode) + 1 + how_many_digits(msu_id) + 2;
    snprintf(data, data_len, "%s %d ", msu_mode, thread_id);

    //TODO: create a template system for MSU such that we can query initial data.
    if (msu_type == DEDOS_SOCKET_HANDLER_MSU_ID) {
        struct socket_handler_init_payload *init_data =
            malloc(sizeof(struct socket_handler_init_payload));

        init_data->port = 8080;
        init_data->domain = AF_INET;
        init_data->type = SOCK_STREAM;
        init_data->protocol = 0;
        init_data->bind_ip = INADDR_ANY;
        /*unsigned char buf[sizeof(struct in_addr)];
        inet_pton(AF_INET, "192.168.0.1", buf);
        init_data->bind_ip = buf;*/
        init_data->target_msu_type = 502; //DEDOS_SSL_REQUEST_ROUTING_MSU_ID;

        size_t len = sizeof(struct socket_handler_init_payload);
        memcpy(data + data_len, init_data, len);
        free(init_data);
        data_len += len;
    } else {
        memcpy(data + data_len, msu_data, strlen(msu_data));
        data_len += strlen(msu_data);
    }

    char create_msu_msg_buffer[sizeof(struct manage_msu_control_payload) + data_len];

    struct manage_msu_control_payload *create_msu_msg =
        (struct manage_msu_control_payload*) create_msu_msg_buffer;
    create_msu_msg->msu_id         = new_msu->msu_id;
    create_msu_msg->msu_type       = new_msu->msu_type;
    create_msu_msg->init_data_size = data_len;
    create_msu_msg->init_data      = data;
    memcpy(create_msu_msg_buffer + sizeof(*create_msu_msg), data, data_len);

    debug("sock:%d, msu_type: %d, msu id: %d, init_data: %s, init_data_size: %d\n",
           runtime_sock,
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
    send_control_msg(runtime_sock, &control_msg);

    //FIXME: assume msu creation goes well.
    //We need some kind of acknowledgement from the runtime
    set_msu(new_msu);
    print_dfg();
    free(buf);

    return 0;
}

int del_msu( int msu_id, int msu_type, int runtime_sock ) {
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

int add_route(int msu_id, int route_id, int runtime_sock){
    // First, update the DFG
    // (this shows that the operation should be valid, and is a stop-gap measure until
    // acknowledgement from the runtime is implemented)

    int rt_index = get_sock_endpoint_index(runtime_sock);
    int rtn = add_route_to_msu_vertex(rt_index, msu_id, route_id);

    if (rtn < 0){
        log_error("Error adding route to vertex! Not proceeding!");
        return -1;
    }

    struct dfg_vertex *msu = get_msu_from_id(msu_id);

    struct manage_msu_control_payload add_route_msg = {
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
        .payload_len = sizeof(add_route_msg),
        .payload = &add_route_msg
    };

    return send_control_msg(runtime_sock, &control_msg);
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

    return send_control_msg(runtime_sock, &control_msg);
}

int add_endpoint(int msu_id, int route_num, unsigned int range_end, int runtime_sock){
    int rt_index = get_sock_endpoint_index(runtime_sock);
    int rtn = dfg_add_route_endpoint(rt_index, route_num, msu_id, range_end);

    if (rtn < 0 ){
        log_error("Error adding route endpoint! Not proceeding!");
        return -1;
    }

    struct dfg_vertex *msu = get_msu_from_id(msu_id);

    enum locality locality;
    uint32_t ip = 0;
    if (get_sock_endpoint_index(msu->scheduling.runtime->sock) != rt_index){
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

    struct dedos_control_msg control_msg = {
        .msg_type = ACTION_MESSAGE,
        .msg_code = ROUTE_ADD_ENDPOINT,
        .header_len = sizeof(control_msg),
        .payload_len = sizeof(route_msg),
        .payload = &route_msg
    };

    return send_control_msg(runtime_sock, &control_msg);
}

int del_endpoint(int msu_id, int route_id, int runtime_sock) {
    int rt_index = get_sock_endpoint_index(runtime_sock);
    int rtn = dfg_del_route_endpoint(rt_index, route_id, msu_id);

    if (rtn < 0) {
        log_error("Error adding route endpoint! Not proceeding!");
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

    return send_control_msg(runtime_sock, &control_msg);
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

    return send_control_msg(runtime_sock, &control_msg);
}
/*
int update_route(int action, int runtime_sock, int from_msu_id, int from_msu_type,
                 int to_msu_id, int to_msu_type, enum locality to_msu_locality, int to_ip) {

    struct dedos_control_msg control_msg;
    char *buf;
    long total_msg_size = 0;

    struct manage_route_control_payload data = {
        .msu_id = to_msu_id,
        .msu_type_id = to_msu_type,
        .locality = to_msu_locality,
        .ipv4 = (uint32_t)to_ip
    };

    struct dedos_control_msg_manage_msu route_update;

    route_update.msu_id = from_msu_id;
    route_update.msu_type = from_msu_type;
    route_update.init_data_size = sizeof(struct msu_control_add_or_del_route);
    route_update.init_data = (struct msu_control_add_or_del_route *) &data;

    debug("DEBUG: Sock %d", runtime_sock);
    debug("DEBUG: MSU id : %d", route_update.msu_id);
    debug("DEBUG: msu_type %d", route_update.msu_type);
    debug("DEBUG: peer_msu_id: %d", data.peer_msu_id);
    debug("DEBUG: peer_msu_type: %u", data.peer_msu_type);
    debug("DEBUG: peer_locality: %u", data.peer_locality);
    debug("DEBUG: peer_ip: %u", data.peer_ip);

    control_msg.msg_type = ACTION_MESSAGE;
    control_msg.msg_code = action;
    control_msg.header_len = sizeof(struct dedos_control_msg); //might be redundant
    control_msg.payload_len =
        sizeof(struct dedos_control_msg_manage_msu) + route_update.init_data_size;

    total_msg_size = sizeof(struct dedos_control_msg) + control_msg.payload_len;

    buf = (char*) malloc(total_msg_size);
    if (!buf) {
        debug("ERROR: Unable to allocate memory for sending control command. %s","");
        return -1;
    }
    memcpy(buf, &control_msg, sizeof(struct dedos_control_msg));
    memcpy(buf + sizeof(struct dedos_control_msg),
           &route_update, sizeof(struct dedos_control_msg_manage_msu));
    memcpy(buf + sizeof(struct dedos_control_msg) + sizeof(struct dedos_control_msg_manage_msu),
           &data, sizeof(struct msu_control_add_or_del_route));

    send_to_runtime(runtime_sock, buf, total_msg_size);

    //TODO: assume msu update goes well. We need some kind of acknowledgement.
    struct dedos_dfg_manage_msg *dfg_manage_msg =
        (struct dedos_dfg_manage_msg *) malloc(sizeof(struct dedos_dfg_manage_msg));

    dfg_manage_msg->msg_type = ACTION_MESSAGE;
    dfg_manage_msg->msg_code = action;
    dfg_manage_msg->header_len = sizeof(struct dedos_dfg_manage_msg);
    dfg_manage_msg->payload_len = sizeof(struct dedos_dfg_update_msu_route_msg);

    struct dedos_dfg_update_msu_route_msg *route_update_msg =
        (struct dedos_dfg_update_msu_route_msg *)
            malloc(sizeof(struct dedos_dfg_update_msu_route_msg));

    route_update_msg->msu_id_from = from_msu_id;
    route_update_msg->msu_id_to = to_msu_id;
    dfg_manage_msg->payload = route_update_msg;

    update_dfg(dfg_manage_msg);

    free(buf);

    return 0;
}*/

int create_worker_thread(int runtime_sock) {
    struct dedos_control_msg control_msg;
    uint32_t num_pinned_threads, num_cores;
    int ret;
    struct dfg_config *dfg;

    dfg = get_dfg();

    int endpoint_index = -1;
    endpoint_index = get_sock_endpoint_index(runtime_sock);
    if (endpoint_index > -1) {
        pthread_mutex_lock(&dfg->dfg_mutex);

        num_pinned_threads = dfg->runtimes[endpoint_index]->num_pinned_threads;
        num_cores = dfg->runtimes[endpoint_index]->num_cores;

        pthread_mutex_unlock(&dfg->dfg_mutex);

        if (num_cores == num_pinned_threads) {
            debug("ERROR: Destination runtime is maxed out. Cores: %u, Pinned threads: %u",
                    num_cores, num_pinned_threads);
            return -1;
        }
    }
    else {
        debug("ERROR: Couldn't find endpoint index for sock: %d", runtime_sock);
        return -1;
    }

    control_msg.msg_type = ACTION_MESSAGE;
    control_msg.msg_code = CREATE_PINNED_THREAD;
    control_msg.payload_len = 0;
    control_msg.header_len = sizeof(struct dedos_control_msg);

    ret = send_control_msg(runtime_sock, &control_msg);

    if (ret == 0) {
        pthread_mutex_lock(&dfg->dfg_mutex);

        dfg->runtimes[endpoint_index]->num_pinned_threads++;
        dfg->runtimes[endpoint_index]->num_threads++;

        pthread_mutex_unlock(&dfg->dfg_mutex);
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
 *
void show_stats(int msu_id) {
    struct dfg_vertex *msu = get_msu_from_id(msu_id);
    int i;

    printf("queue_item_processed\n");
    printf("value, timestamp\n");
    for (i = 0; i < TIME_SLOTS; ++i) {
        if (msu->statistics->queue_item_processed->timestamp[i] > 0) {
            printf("%d, %d\n",
                    msu->statistics->queue_item_processed->data[i],
                    msu->statistics->queue_item_processed->timestamp[i]);
        }
    }

    printf("data_queue_size\n");
    printf("value, timestamp\n");
    for (i = 0; i < TIME_SLOTS; ++i) {
        if (msu->statistics->data_queue_size->timestamp[i] > 0) {
            printf("%d, %d\n",
                    msu->statistics->data_queue_size->data[i],
                    msu->statistics->data_queue_size->timestamp[i]);
        }
    }

    printf("memory_allocated\n");
    printf("value, timestamp\n");
    for (i = 0; i < TIME_SLOTS; ++i) {
        if (msu->statistics->memory_allocated->timestamp[i] > 0) {
            printf("%d, %d\n",
                    msu->statistics->memory_allocated->data[i],
                    msu->statistics->memory_allocated->timestamp[i]);
        }
    }
}
*/
