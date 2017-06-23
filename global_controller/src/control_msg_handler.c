/**
 * parse all incoming messages coming from global control over
 * tcp conn from the master
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "dedos_statistics.h"
#include "control_msg_handler.h"
#include "stat_msg_handler.h"
#include "communication.h"
#include "controller_tools.h"
#include "dfg.h"

#define MSG_SIZE 64

#define MAX_SEND_BUFLEN 8192

char stat_msg_buffer[MAX_STAT_BUFF_SIZE];

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
            //FIXME: either unknown runtime must send detailed profile info, either we forbid
            //registration of unknown runtime.
	    log_debug("Got a GET_INIT_CONFIG message");
            count = show_connected_peers();

            if (count == 1) {
                debug("%s", "1 connected runtime: no need to sync with itself. Quitting.");
                return;
            }
	    debug("Connected peers count including requesting runtime: %d",count);

            all_peer_ips = malloc(count * sizeof(uint32_t));
            if (!all_peer_ips) {
                debug("ERROR: Unable to allocate memory for peer list %s","");
		return;
            }

            //assume one of the peer will also be the recipient
	    memset(all_peer_ips, 0, count * sizeof(uint32_t));

            count = get_connected_peer_ips(all_peer_ips);

	    int t;
	    char ipstr[INET_ADDRSTRLEN];
	    for(t = 0; t < count; t++){
	        ipv4_to_string(ipstr, all_peer_ips[t] );
		debug("connected list: %s", ipstr);
            }

            dst_runtime_ip = get_sock_endpoint_ip(runtime_sock);
            if (dst_runtime_ip == -1) {
                debug("Could not find ip associated with socket %d", runtime_sock);
                return;
            }

            to_send_peer_ips = malloc((count - 1) * sizeof(uint32_t));
            if (!to_send_peer_ips) {
                debug("ERROR: Unable to allocate memory for peer list %s","");
		free(all_peer_ips);
	        return;
            }
	    memset(to_send_peer_ips, 0, (count-1) * sizeof(uint32_t));

            int c;
            int send_count = 0;
            for (c = 0; c < count; c++) {
                if (all_peer_ips[c] != dst_runtime_ip) {
                    to_send_peer_ips[send_count] = all_peer_ips[c];
		    ipv4_to_string(ipstr, to_send_peer_ips[send_count] );
		    debug("to send: %s", ipstr);
      	            send_count++;
                }
            }

	    if(send_count != count -1){
                debug("Send count - count != -1");
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
            response->payload_len = (send_count * sizeof(uint32_t));

            response->payload = to_send_peer_ips;
            response->header_len = sizeof(struct dedos_control_msg);

            debug("DEBUG: Payload len of response list: %u", response->payload_len);

            send_control_msg(runtime_sock, response);


            free(response);
            free(all_peer_ips);
            free(to_send_peer_ips);

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

        case STATISTICS_UPDATE:;

            struct stats_control_payload stats;
            int rtn = deserialize_stat_payload(control_msg->payload, &stats);

            if ( rtn < 0 ) {
                log_error("Error deserializing stats payload");
                break;
            }

            process_stats_msg(&stats, runtime_sock);

            break;

        default:
            debug("ERROR: Unknown msg_type: %u",control_msg->msg_code);
            break;
    }
    return;
}
