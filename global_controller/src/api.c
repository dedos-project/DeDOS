#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "api.h"
#include "dfg.h"
#include "control_protocol.h"
#include "controller_tools.h"
#include "communication.h"

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
        pthread_mutex_lock(dfg->dfg_mutex);

        new_msu = malloc(sizeof(struct dfg_vertex));
        if (new_msu == NULL) {
            debug("ERROR: %s", "could not allocate memory for new msu");
        }

        memcpy(new_msu->msu_mode, msu_mode, strlen(msu_mode));
        new_msu->msu_type = msu_type;
        new_msu->msu_id = msu_id;

        struct msu_scheduling *s = NULL;
        s = malloc(sizeof(struct msu_scheduling));

        s->runtime = dfg->runtimes[endpoint_index];
        s->thread_id = thread_id;
        new_msu->scheduling = s;

        num_pinned_threads = dfg->runtimes[endpoint_index]->num_pinned_threads;

        pthread_mutex_unlock(dfg->dfg_mutex);

        if (num_pinned_threads == 0 || num_pinned_threads < s->thread_id) {
            debug("ERROR: Destination runtime has %d worker threads. Can't fit new msu on thread %d",
                  num_pinned_threads, s->thread_id);
            free(s);
            free(new_msu);
            return -1;
        }
    } else {
        debug("ERROR: Couldn't find endpoint index for sock: %d", runtime_sock);
        return -1;
    }

    // [mode][space][thread_id][\0]
    int data_len = strlen(msu_data) + 1 + strlen(msu_mode) + 1 + how_many_digits(msu_id) + 1;
    char data[data_len];
    snprintf(data, data_len, "%s %d %s", msu_mode, thread_id, msu_data);

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
    set_msu(new_msu);
    print_dfg();
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
    for (i = 0; i < TIME_SLOTS; ++i) {
        printf("%d, %d\n",
                msu->statistics->queue_item_processed->data[i],
                msu->statistics->queue_item_processed->timestamp[i]);
    }

    printf("data_queue_size\n");
    printf("value, timestamp\n");
    for (i = 0; i < TIME_SLOTS; ++i) {
        printf("%d, %d\n",
                msu->statistics->data_queue_size->data[i],
                msu->statistics->data_queue_size->timestamp[i]);
    }

    printf("memory_allocated\n");
    printf("value, timestamp\n");
    for (i = 0; i < TIME_SLOTS; ++i) {
        printf("%d, %d\n",
                msu->statistics->memory_allocated->data[i],
                msu->statistics->memory_allocated->timestamp[i]);
    }
}
