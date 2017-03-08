/**
 * parse all incoming messages coming from global control over
 * tcp conn from the master
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "control_msg_handler.h"
#include "stat_msg_handler.h"
#include "communication.h"
#include "controller_tools.h"
#include "dfg.h"

#define MSG_SIZE 64

#define MAX_SEND_BUFLEN 8192

void process_runtime_msg(char *cmd, int runtime_sock) {
    int count;
    int endpoint_index = -1;
    uint32_t dst_runtime_ip = 0;
    uint32_t *all_peer_ips = NULL;
    uint32_t *to_send_peer_ips = NULL;
    char *sendbuf;
    struct dedos_control_msg *control_msg;
    struct dfg_config *dfg;

    dfg = get_dfg();

    control_msg = (struct dedos_control_msg*)cmd;
    if (control_msg->payload_len) {
        control_msg->payload = cmd + control_msg->header_len;
    }

    switch (control_msg->msg_code) {
        case GET_INIT_CONFIG:
            /* send initial info to the runtime at the end of runtime_sock */
            debug("Recvd core count from runtime: %d", control_msg->payload_len);

            endpoint_index = get_sock_endpoint_index(runtime_sock);

            pthread_mutex_lock(dfg->dfg_mutex);

            if (endpoint_index > -1) {
                dfg->runtimes[endpoint_index]->num_cores = control_msg->payload_len;
                // just the one main thread
                dfg->runtimes[endpoint_index]->num_threads = 1;
                dfg->runtimes[endpoint_index]->num_pinned_threads = 0;

            } else {
                debug("ERROR: Couldn't find endpoint index for sock: %d", runtime_sock);
            }

            pthread_mutex_unlock(dfg->dfg_mutex);

            count = show_connected_peers();

            if (count == 1) {
                debug("%s", "1 connected runtime: no need to sync with itself. Quitting.");
                return;
            }

            all_peer_ips = malloc(count * sizeof(uint32_t));
            //assume one of the peer will also be the recipient
            to_send_peer_ips = malloc((count - 1) * sizeof(uint32_t));
            if (!all_peer_ips || !to_send_peer_ips) {
                debug("ERROR: Unable to allocate memory for peer list %s","");
            }

            get_connected_peer_ips(all_peer_ips);
            dst_runtime_ip = get_sock_endpoint_ip(runtime_sock);

            if (dst_runtime_ip == -1) {
                debug("Could not find ip associated with socket %d", runtime_sock);
                return;
            }

            int c;
            int d = 0;
            for (c = 0; c < count; c++) {
                if (all_peer_ips[c] != dst_runtime_ip) {
                    to_send_peer_ips[d] = all_peer_ips[c];
                    d++;
                }
            }

            //prepare response
            struct dedos_control_msg *response;
            response = malloc(sizeof(struct dedos_control_msg));
            if (!response) {
                debug("ERROR: Unable to allocate memory for dedos_control_msg for IP list %s","");
                free(all_peer_ips);
                free(to_send_peer_ips);
                break;
            }
            response->msg_type = RESPONSE_MESSAGE;
            response->msg_code = SET_DEDOS_RUNTIMES_LIST;
            response->payload_len = (count * sizeof(uint32_t));

            response->payload = to_send_peer_ips;
            response->header_len = sizeof(struct dedos_control_msg);

            debug("DEBUG: Payload len of response: %u", response->payload_len);

            sendbuf = malloc(sizeof(struct dedos_control_msg) + ((count-1) * sizeof(uint32_t)));
            if (!sendbuf) {
                debug("ERROR: Failed to allocate sendbuf memory %s","");
                free(response);
                free(all_peer_ips);
                free(to_send_peer_ips);
            }

            memcpy(sendbuf, response, sizeof(struct dedos_control_msg));
            memcpy(sendbuf + sizeof(struct dedos_control_msg), to_send_peer_ips, ((count-1) * sizeof(uint32_t)));

            send_to_runtime(runtime_sock, sendbuf, sizeof(struct dedos_control_msg) + ((count-1) * sizeof(uint32_t)));

            free(response);
            free(all_peer_ips);
            free(to_send_peer_ips);
            free(sendbuf);

            break;

        case GET_MSU_LIST:

            if(control_msg->msg_type == RESPONSE_MESSAGE){
                int result_count;

                result_count = control_msg->payload_len / sizeof(struct msu_thread_info);
                struct msu_thread_info *info = control_msg->payload;
                int i;
                debug("DEBUG: Received following MSU placement info from runtime %s", "");
                debug("---------------------------------------%s","\n");
                for (i = 0; i < result_count; i++) {
                    debug("MSU ID: %d, thread_id: %lu", info[i].msu_id,
                            info[i].thread_id);
                }
                debug("DEBUG:----------------------------------------%s", "");
                //TODO Maybe use received msu_list somehow later

            } else {
                debug("ERROR: Master should not be getting a request for MSU_LIST! %s","");
            }
            break;

        case STATISTICS_UPDATE:
            //process received stats update here
            if (control_msg->msg_type == ACTION_MESSAGE) {
                unsigned int array_len = control_msg->payload_len / sizeof(struct msu_stats_data);
                int i;
                struct msu_stats_data *rcvd_stats_array = (struct msu_stats_data*)control_msg->payload;

                /*
                debug("Received stats: %s","");
                for (i = 0; i < array_len; ++i) {
                    debug("msu_id: %d, items_processed: %u, mem_usage: %u, q_size: %u",
                            rcvd_stats_array[i].msu_id,
                            rcvd_stats_array[i].queue_item_processed,
                            rcvd_stats_array[i].memory_allocated,
                            rcvd_stats_array[i].data_queue_size
                         );
                }
                */
                process_stats_msg(rcvd_stats_array, runtime_sock);
            } else {
                debug("ERROR: Wrong msg type set for STATISTICS update %u", control_msg->msg_type);
            }

            break;

        default:
            debug("ERROR: Unknown msg_type: %u",control_msg->msg_code);
            break;
    }
    return;
}
