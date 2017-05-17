#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#include "dfg.h"
#include "logging.h"
#include "control_protocol.h"
#include "controller_tools.h"
#include "scheduling.h"

/* Display information about runtimes */
int show_connected_peers(void) {
    char ipstr[INET_ADDRSTRLEN];
    int count = 0;
    int i;

    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    debug("INFO: %s", "Current open runtime peer sockets ->");

    for (i = 0; i < dfg->runtimes_cnt; ++i) {
        if (dfg->runtimes[i]->sock != -1) {
            ipv4_to_string(ipstr, dfg->runtimes[i]->ip);
            debug("INFO => Socket: %d, IP: %s, Cores: %u, Pinned Threads: %u  Total Threads: %u",
                  dfg->runtimes[i]->sock,
                  ipstr,
                  dfg->runtimes[i]->num_cores,
                  dfg->runtimes[i]->num_pinned_threads,
                  dfg->runtimes[i]->num_threads);
            count++;
        }
    }

    return count;
}

/**
 * Get runtime pointer from ID
 * @param integer runtime_id
 * @return struct runtime_endpoint *runtime
 */
struct runtime_endpoint *get_runtime_from_id(int runtime_id) {
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    int i;
    for (i = 0; i < dfg->runtimes_cnt; ++i) {
        if (dfg->runtimes[i]->id == runtime_id) {
            return dfg->runtimes[i];
        }
    }

    return NULL;
}

/**
 * Get runtimes IPs
 * @param uint32_t *peer_ips
 * @return void
 */
void get_connected_peer_ips(uint32_t *peer_ips) {
    int count = 0;
    int i;
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    for (i = 0; i < dfg->runtimes_cnt; ++i) {
        if (dfg->runtimes[i]->sock != -1) {
            peer_ips[count] = dfg->runtimes[i]->ip;
            count++;
        }
    }

}

/**
 * Get runtime IP from socket
 * @param integer sock
 * @return uint32_t runtime ip
 */
uint32_t get_sock_endpoint_ip(int sock) {
    int i;
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    for (i = 0; i < dfg->runtimes_cnt; ++i) {
        if (dfg->runtimes[i]->sock == sock) {
            return dfg->runtimes[i]->ip;
        }
    }

    //TODO answer an unsigned integer here (0?)
    return -1;
}

/**
 * Get runtime index from socket
 * @param integer sock
 * @return integer runtime index
 */
int get_sock_endpoint_index(int sock) {
    int i;
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    for (i = 0; i < dfg->runtimes_cnt; ++i) {
        if (dfg->runtimes[i]->sock == sock) {
            return i;
        }
    }

    return -1;
}


/**
 * Retrieve all MSU ids of a given type
 * @param integer type
 * @return struct msus_of_type *msu_of_type
 */
struct msus_of_type *get_msus_from_type(int type) {
    int msu_ids[MAX_MSU];
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();
    struct msus_of_type *s = NULL;
    s = malloc(sizeof(struct msus_of_type));

    int p = 0;
    int c;
    for (c = 0; c < dfg->vertex_cnt; ++c) {
        if (dfg->vertices[c]->msu_type == type) {
            msu_ids[p] = dfg->vertices[c]->msu_id;
            p++;
        }
    }

    s->msu_ids = msu_ids;
    s->num_msus = p;
    s->type = type;

    return s;
}

/**
 * Retrieve dfg vertex given a msu ID
 * @param integer msu_id
 * @return struct dfg_vertex *msu
 */
struct dfg_vertex *get_msu_from_id(int msu_id) {
    struct dfg_vertex *msu = NULL;
    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    int i;
    for (i = 0; i < dfg->vertex_cnt; ++i) {
        if (dfg->vertices[i]->msu_id == msu_id) {
            msu = dfg->vertices[i];
            break;
        }
    }

    return msu;
}

/**
 * Overwrite or create a MSU in the DFG
 * @param dfg_vertex msu: pointer to the new vertex struct
 * @return none
 */
void set_msu(struct dfg_vertex *msu) {
    debug("DEBUG: adding msu %d to the graph", msu->msu_id);

    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    int new_id = -1;

    int m;
    for (m = 0; m < dfg->vertex_cnt; ++m) {
        if (dfg->vertices[m]->msu_id == msu->msu_id) {
            free(dfg->vertices[m]);
            new_id = m;
            break;
        }
    }

    new_id = (new_id != -1)? new_id : dfg->vertex_cnt;
    dfg->vertices[new_id] = msu;
    dfg->vertex_cnt++;

    dfg->vertices[new_id]->statistics = malloc(sizeof(struct msu_statistics_data));
    dfg->vertices[new_id]->statistics->queue_item_processed = malloc(sizeof(struct timeserie));
    dfg->vertices[new_id]->statistics->queue_item_processed->timepoint = 0;
    memset(dfg->vertices[new_id]->statistics->queue_item_processed->data, 0, TIME_SLOTS);
    memset(dfg->vertices[new_id]->statistics->queue_item_processed->timestamp, 0, TIME_SLOTS);
    dfg->vertices[new_id]->statistics->data_queue_size = malloc(sizeof(struct timeserie));
    dfg->vertices[new_id]->statistics->data_queue_size->timepoint = 0;
    memset(dfg->vertices[new_id]->statistics->data_queue_size->data, 0, TIME_SLOTS);
    memset(dfg->vertices[new_id]->statistics->data_queue_size->timestamp, 0, TIME_SLOTS);
    dfg->vertices[new_id]->statistics->memory_allocated = malloc(sizeof(struct timeserie));
    dfg->vertices[new_id]->statistics->memory_allocated->timepoint = 0;
    memset(dfg->vertices[new_id]->statistics->memory_allocated->data, 0, TIME_SLOTS);
    memset(dfg->vertices[new_id]->statistics->memory_allocated->timestamp, 0, TIME_SLOTS);
}

/* Big mess of create, updates, etc */
void update_dfg(struct dedos_dfg_manage_msg *update_msg) {
    debug("DEBUG: updating MSU for action: %d", update_msg->msg_code);

    struct dfg_config *dfg = NULL;
    dfg = get_dfg();

    switch (update_msg->msg_code) {
            /*
        case MSU_ROUTE_ADD: {
            //TODO: don't assume that the src/dst msu did not exist in the table before.
            struct dedos_dfg_update_msu_route_msg *update;
            update = (struct dedos_dfg_update_msu_route_msg *) update_msg->payload;

            struct dfg_vertex *msu_from = dfg->vertices[update->msu_id_from - 1];
            struct dfg_vertex *msu_to = dfg->vertices[update->msu_id_to - 1];

            //TODO: handle "holes" in the list, due to MSU_ROUTE_DEL
            msu_from->msu_dst_list[msu_from->num_dst_types] = msu_to;
            msu_from->num_dst_types++;

            msu_to->msu_src_list[msu_to->num_src_types] = msu_from;
            msu_to->num_src_types++;

            free(update_msg);
            free(update);
            break;
        }
            */

        case RUNTIME_ENDPOINT_ADD: {
            //because both in toload and preload mode we have already been adding some
            //runtimes in the DFG, we need to check if the new runtime isn't actually
            //an "expected" one, while allowing totally new runtimes to register.

            if (dfg->runtimes_cnt == MAX_RUNTIMES) {
                debug("ERROR: %s", "Number of runtimes limit reached.");
                break;
            }

            struct dedos_dfg_add_endpoint_msg *update;
            update = (struct dedos_dfg_add_endpoint_msg *) update_msg->payload;

            debug("DEBUG: registering endpoint at %s", update->runtime_ip);

            struct runtime_endpoint *r = NULL;

            int i;
            for (i = 0; i < dfg->runtimes_cnt; ++i) {
                char runtime_ip[INET_ADDRSTRLEN];
                ipv4_to_string(runtime_ip, dfg->runtimes[i]->ip);

                //We assume only one runtime per ip
                if (strcmp(runtime_ip, update->runtime_ip) == 0) {
                    r = dfg->runtimes[i];
                    break;
                }
            }

            if (r == NULL) {
                r = malloc(sizeof(struct runtime_endpoint));

                string_to_ipv4(update->runtime_ip, &(r->ip));

                dfg->runtimes[dfg->runtimes_cnt] = r;
                dfg->runtimes_cnt++;
            }

            r->sock = update->runtime_sock;

            free(update_msg);
            free(update);
            break;
        }

        default:
            debug("DEBUG: unrecognized update action: %d", update_msg->msg_code);
            break;
    }

}
