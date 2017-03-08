#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "api.h"
#include "dfg.h"
#include "control_protocol.h"
#include "communication.h"

int add_msu(char *data, struct dfg_vertex *new_msu, int runtime_sock) {
    char *buf;
    long total_msg_size = 0;
    struct dedos_control_msg control_msg;
    struct dfg_config *dfg;

    dfg = get_dfg();

    int endpoint_index = -1;
    uint32_t num_pinned_threads;
    endpoint_index = get_sock_endpoint_index(runtime_sock);
    if (endpoint_index > -1) {
        pthread_mutex_lock(dfg->dfg_mutex);

        num_pinned_threads = dfg->runtimes[endpoint_index]->num_pinned_threads;
        new_msu->msu_runtime = dfg->runtimes[endpoint_index];

        pthread_mutex_unlock(dfg->dfg_mutex);

        if (num_pinned_threads == 0 || num_pinned_threads < new_msu->thread_id) {
            debug("ERROR: Destination runtime has %d worker threads. Can't fit new msu on thread %d",
                  num_pinned_threads, new_msu->thread_id);
            return -1;
        }
    }
    else {
        debug("ERROR: Couldn't find endpoint index for sock: %d", runtime_sock);
        return -1;
    }

    struct dedos_control_msg_manage_msu create_msu_msg;
    create_msu_msg.msu_id         = new_msu->msu_id;
    create_msu_msg.msu_type       = new_msu->msu_type;
    create_msu_msg.init_data_size = strlen(data);
    create_msu_msg.init_data      = data;

    debug("DEBUG: Sock %d\n", runtime_sock);
    debug("DEBUG: msu_type %d\n", create_msu_msg.msu_type);
    debug("DEBUG: MSU id : %d\n", create_msu_msg.msu_id);
    debug("DEBUG: init_data : %s\n", create_msu_msg.init_data);
    debug("DEBUG: init_data_size: %d\n", create_msu_msg.init_data_size);

    control_msg.msg_type = ACTION_MESSAGE;
    control_msg.msg_code = CREATE_MSU;
    control_msg.header_len = sizeof(struct dedos_control_msg); //might be redundant
    control_msg.payload_len =
        sizeof(struct dedos_control_msg_manage_msu) + create_msu_msg.init_data_size;

    total_msg_size = sizeof(struct dedos_control_msg) + control_msg.payload_len;
    buf = (char*) malloc(total_msg_size);
    if(!buf){
        debug("ERROR: Unable to allocate memory for sending control command. %s","");
        return -1;
    }
    memcpy(buf, &control_msg, sizeof(struct dedos_control_msg));
    memcpy(buf + sizeof(struct dedos_control_msg), &create_msu_msg, sizeof(create_msu_msg));
    memcpy(buf + sizeof(struct dedos_control_msg) + sizeof(create_msu_msg), data, strlen(data));

    send_to_runtime(runtime_sock, buf, total_msg_size);
    //FIXME: assume msu creation goes well.
    //We need some kind of acknowledgement from the runtime
#if NO_DFG != 1
    set_msu(new_msu);
    print_dfg();
#endif
    free(buf);

    return 0;
}

int update_route(int action, int runtime_sock, int from_msu_id, int from_msu_type,
                 int to_msu_id, int to_msu_type, int to_msu_locality, int to_ip) {

    struct dedos_control_msg control_msg;
    char *buf;
    long total_msg_size = 0;

    struct msu_control_add_or_del_route data = {
            .peer_msu_id = to_msu_id,
            .peer_msu_type = to_msu_type,
            .peer_locality = to_msu_locality,
            .peer_ip = to_ip
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
#if NO_DFG != 1
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
#endif
    free(buf);

    return 0;
}

int create_worker_thread(int runtime_sock) {
    struct dedos_control_msg control_msg;
    uint32_t num_pinned_threads, num_cores;
    int ret;
    struct dfg_config *dfg;

    dfg = get_dfg();

    int endpoint_index = -1;
    endpoint_index = get_sock_endpoint_index(runtime_sock);
    if (endpoint_index > -1) {
        pthread_mutex_lock(dfg->dfg_mutex);

        num_pinned_threads = dfg->runtimes[endpoint_index]->num_pinned_threads;
        num_cores = dfg->runtimes[endpoint_index]->num_cores;

        pthread_mutex_unlock(dfg->dfg_mutex);

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

    ret = send_to_runtime(runtime_sock, &control_msg, sizeof(struct dedos_control_msg));
    if (ret == 0) {
        pthread_mutex_lock(dfg->dfg_mutex);

        dfg->runtimes[endpoint_index]->num_pinned_threads++;
        dfg->runtimes[endpoint_index]->num_threads++;

        pthread_mutex_unlock(dfg->dfg_mutex);
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
        pthread_mutex_lock(dfg->dfg_mutex);
        strncpy(endpoint_msg->runtime_ip, dfg->global_ctl_ip, INET_ADDRSTRLEN);
        pthread_mutex_unlock(dfg->dfg_mutex);

    } else {
        strncpy(endpoint_msg->runtime_ip, runtime_ip, INET_ADDRSTRLEN);
    }

    dfg_manage_msg->payload = endpoint_msg;

    update_dfg(dfg_manage_msg);
    print_dfg();

    return 0;
}
