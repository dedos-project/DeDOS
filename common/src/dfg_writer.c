#include <string.h>
#include "ip_utils.h"
#include "dfg.h"
#include "jsmn.h"
#include "logging.h"

#define MAX_JSON_LEN 16384

jsmn_parser jp;

/**
 * Compare two JSON strings
 * @param const char *json a JSON string
 * @param jsmntok_t *tok pointer to a jsmn token
 * @param const char *s what to compare with
 * @return integer match / no match
 */
int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
    strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

/**
 * Details about a JSMN error
 * @param int error code
 * @return none
 */
void json_parse_error(int err) {
    switch(err) {
        case JSMN_ERROR_INVAL:
            debug("%s", "Bad token, JSON string is corrupted");
            break;
        case JSMN_ERROR_NOMEM:
            debug("%s", "not enough tokens, JSON string is too large");
            break;
        case JSMN_ERROR_PART:
            debug("%s", "JSON string is too short, expecting more JSON data");
            break;
    }
}

/**
 * Make a pass over a JSON string and ensure it is made of valid tokens
 * @param jsmntok_t *t pointer to a token
 * @param int r number of token returned by json_parse()
 * @return int valid / notvalid
 */
int json_test(jsmntok_t *t, int r) {
    if (r == 0 || t[0].type != JSMN_OBJECT) {
        debug("ERROR: %s", "The top-level element should be an object!");
        return -1;
    }

    int j;
    for (j = 0; j < r; j++) {
        switch(t[j].type) {
            case JSMN_ARRAY:
                break;
            case JSMN_OBJECT:
                break;
            case JSMN_PRIMITIVE:
                break;
            case JSMN_STRING:
                break;
            default:
                debug("Unknown json element: %d", t[j].type);
                return -1;
            break;
        }
    }

    return 0;
}

/**
 * Parse the DFG and format it as a JSON string
 * @return none
 */
void dump_json(struct dfg_config *dfg_ref) {
    struct dfg_config dfg = *dfg_ref;
    char json_string[MAX_JSON_LEN];
    int json_len = 0;
    int field_len;

    //note that all snprintf must add 1 bytes for the \0, which it includes automatically
    json_len += snprintf(json_string, 2, "{");

    /**
     *  APPLICATION
     */

    //application name
    field_len = 21 + strlen(dfg.application_name) + 1;
    json_len += snprintf(json_string + json_len,
                         field_len,
                         "\"application_name\":\"%s\"",
                         dfg.application_name);
    json_len += snprintf(json_string + json_len, 3, ", ");

    //applicatin dealine
    field_len = 25 + how_many_digits(dfg.application_deadline) + 1;
    json_len += snprintf(json_string + json_len,
                         field_len,
                         "\"application_deadline\":\"%d\"",
                         dfg.application_deadline);
    json_len += snprintf(json_string + json_len, 3, ", ");

    //global ctl ip
    field_len = 18 + strlen(dfg.global_ctl_ip) + 1;
    json_len += snprintf(json_string + json_len,
                         field_len,
                         "\"global_ctl_ip\":\"%s\"",
                         dfg.global_ctl_ip);
    json_len += snprintf(json_string + json_len, 3, ", ");

    //global ctl port
    field_len = 20 + how_many_digits(dfg.global_ctl_port) + 1;
    json_len += snprintf(json_string + json_len,
                         field_len,
                         "\"global_ctl_port\":\"%d\"",
                         dfg.global_ctl_port);

    /**
     *  RUNTIMES
     */
    if (dfg.runtimes_cnt > 0) {
        json_len += snprintf(json_string + json_len, 3, ", ");
        field_len = 12 + 1;
        json_len += snprintf(json_string + json_len,
                             field_len,
                             "\"runtimes\":[");

        int r;
        for (r = 0; r < dfg.runtimes_cnt; ++r) {
            if (r > 0) {
                json_len += snprintf(json_string + json_len, 3, ", ");
            }

            json_len += snprintf(json_string + json_len, 2, "{");

            //id
            field_len = 7 + how_many_digits(dfg.runtimes[r]->id) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"id\":\"%d\"",
                                 dfg.runtimes[r]->id);
            json_len += snprintf(json_string + json_len, 3, ", ");

            if (dfg.runtimes[r]->sock != -1) {
                //socket
                field_len = 9 + how_many_digits(dfg.runtimes[r]->sock) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"sock\":\"%d\"",
                                     dfg.runtimes[r]->sock);
                json_len += snprintf(json_string + json_len, 3, ", ");
            }

            field_len = 11;
            json_len += snprintf(json_string + json_len, field_len, "\"routes\":[");
            // Routes
            for (int route_i = 0; route_i < dfg.runtimes[r]->num_routes; route_i++){
               struct dfg_route *route = dfg.runtimes[r]->routes[route_i];
               if (route_i == 0)
                   json_len += snprintf(json_string + json_len, 2, "{");
               else
                   json_len += snprintf(json_string + json_len, 3, ",{");

               field_len = 9 + how_many_digits(route->route_id);
               json_len += snprintf(json_string+json_len, field_len, "\"id\":\"%d\",", route->route_id);

               field_len = 17;
               json_len += snprintf(json_string+json_len, field_len, "\"destinations\":[");

               for (int dest_i = 0; dest_i < route->num_destinations; dest_i++){
                    if (dest_i > 0)
                        json_len += snprintf(json_string + json_len, 3, ", ");
                    field_len = 3 + how_many_digits(route->destinations[dest_i]->msu_id);
                    json_len += snprintf(json_string + json_len, field_len, "\"%d\"", route->destinations[dest_i]->msu_id);
               }
               json_len += snprintf(json_string+json_len, 3, "]}");
            }
            json_len += snprintf(json_string + json_len, 3, "],");

            //ip
            char ipstr[INET_ADDRSTRLEN];
            ipv4_to_string(&ipstr, dfg.runtimes[r]->ip);
            field_len = 7 + strlen(ipstr) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"ip\":\"%s\"",
                                 ipstr);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //port
            field_len = 9 + how_many_digits(dfg.runtimes[r]->port) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"port\":\"%d\"",
                                 dfg.runtimes[r]->port);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //number of cores
            field_len = 14 + how_many_digits(dfg.runtimes[r]->num_cores) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"num_cores\":\"%d\"",
                                 dfg.runtimes[r]->num_cores);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //dram
            field_len = 9 + how_many_digits(dfg.runtimes[r]->dram) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"dram\":\"%d\"",
                                 dfg.runtimes[r]->dram);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //io_network_bw
            field_len = 18 + how_many_digits(dfg.runtimes[r]->io_network_bw) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"io_network_bw\":\"%d\"",
                                 dfg.runtimes[r]->io_network_bw);

            //current state
            if (dfg.runtimes[r]->current_alloc != NULL) {
                int timestamp = time(NULL);

                json_len += snprintf(json_string + json_len, 3, ", ");
                json_len += snprintf(json_string + json_len, 2, "{");

                //timestamp
                field_len = 14 + how_many_digits(timestamp) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"timestamp\":\"%d\"",
                                     timestamp);
                json_len += snprintf(json_string + json_len, 3, ", ");

                //non-blocking threads
                field_len = 25 + how_many_digits(dfg.runtimes[r]->num_pinned_threads) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"non_blocking_workers\":\"%d\"",
                                     dfg.runtimes[r]->num_pinned_threads);
                json_len += snprintf(json_string + json_len, 3, ", ");

                //blocking threads
                int num_blocking =
                    dfg.runtimes[r]->num_threads - dfg.runtimes[r]->num_pinned_threads;
                field_len = 21 + how_many_digits(num_blocking) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"blocking_workers\":\"%d\"",
                                     num_blocking);
                json_len += snprintf(json_string + json_len, 3, ", ");

                //used cores
                field_len = 10 + how_many_digits(dfg.runtimes[r]->current_alloc->num_cores) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"cores\":\"%d\"",
                                     dfg.runtimes[r]->current_alloc->num_cores);
                json_len += snprintf(json_string + json_len, 3, ", ");

                //bissection bw
                field_len = 19 + how_many_digits(dfg.runtimes[r]->current_alloc->bissection_bw) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"bissection_bw\":\"%d\"",
                                     dfg.runtimes[r]->current_alloc->bissection_bw);
                json_len += snprintf(json_string + json_len, 3, ", ");

                //egress bw
                field_len = 14 + how_many_digits(dfg.runtimes[r]->current_alloc->egress_bw) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"egress_bw\":\"%d\"",
                                     dfg.runtimes[r]->current_alloc->egress_bw);
                json_len += snprintf(json_string + json_len, 3, ", ");

                //ingress bw
                field_len = 15 + how_many_digits(dfg.runtimes[r]->current_alloc->ingress_bw) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"ingress_bw\":\"%d\"",
                                     dfg.runtimes[r]->current_alloc->ingress_bw);
                json_len += snprintf(json_string + json_len, 3, ", ");

                //num msus
                field_len = 15 + how_many_digits(dfg.runtimes[r]->current_alloc->num_msu) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"num_msus\":\"%d\"",
                                     dfg.runtimes[r]->current_alloc->num_msu);

                json_len += snprintf(json_string + json_len, 2, "}");
            }

            json_len += snprintf(json_string + json_len, 2, "}");
        }

        json_len += snprintf(json_string + json_len, 2, "]");
    }

    /**
     *  MSUS
     */

    if (dfg.vertex_cnt > 0 ) {
        json_len += snprintf(json_string + json_len, 3, ", ");

        field_len = 8 + 1;
        json_len += snprintf(json_string + json_len,
                             field_len,
                             "\"MSUs\":[");

        int m;
        for (m = 0; m < dfg.vertex_cnt; ++m) {
            if (m > 0) {
                json_len += snprintf(json_string + json_len, 3, ", ");
            }

            json_len += snprintf(json_string + json_len, 2, "{");
            //id
            field_len = 7 + how_many_digits(dfg.vertices[m]->msu_id) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"id\":\"%d\"",
                                 dfg.vertices[m]->msu_id);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //name
            field_len = 9 + strlen(dfg.vertices[m]->msu_name) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"name\":\"%s\"",
                                 dfg.vertices[m]->msu_name);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //type
            field_len = 9 + how_many_digits(dfg.vertices[m]->msu_type) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"type\":\"%d\"",
                                 dfg.vertices[m]->msu_type);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //mode
            field_len = 17 + strlen(dfg.vertices[m]->msu_mode) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"working-mode\":\"%s\"",
                                 dfg.vertices[m]->msu_mode);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //profiling
            field_len = 13 + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"profiling\":{");

            //wcet
            field_len = 9 + how_many_digits(dfg.vertices[m]->profiling.wcet) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"wcet\":\"%d\"",
                                 dfg.vertices[m]->profiling.wcet);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //footprint
            field_len = 18 + how_many_digits(dfg.vertices[m]->profiling.dram) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"mem_footprint\":\"%d\"",
                                 dfg.vertices[m]->profiling.dram);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //tx_node_local
            field_len = 18 + how_many_digits(dfg.vertices[m]->profiling.tx_node_local) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"tx_node_local\":\"%d\"",
                                 dfg.vertices[m]->profiling.tx_node_local);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //tx_core_local
            field_len = 18 + how_many_digits(dfg.vertices[m]->profiling.tx_core_local) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"tx_core_local\":\"%d\"",
                                 dfg.vertices[m]->profiling.tx_core_local);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //tx_node_remote
            field_len = 19 + how_many_digits(dfg.vertices[m]->profiling.tx_node_remote) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"tx_node_remote\":\"%d\"",
                                 dfg.vertices[m]->profiling.tx_node_remote);

            json_len += snprintf(json_string + json_len, 3, "},");

            //meta routing
            field_len = 17 + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"meta_routing\":{");

            //sources
            int has_sources = 0;
            if (strncmp(dfg.vertices[m]->vertex_type, "entry", strlen("entry\0")) != 0 &&
                dfg.vertices[m]->meta_routing.num_src_types > 0) {

                has_sources = 1;

                field_len = 17 + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"sources_types\":[");

                int s;
                for (s = 0; s < dfg.vertices[m]->meta_routing.num_src_types; ++s) {
                    if (s > 0) {
                        json_len += snprintf(json_string + json_len, 3, ", ");
                    }

                    //id
                    field_len = 2 + how_many_digits(dfg.vertices[m]->meta_routing.src_types[s]) + 1;
                    json_len += snprintf(json_string + json_len,
                                         field_len,
                                         "\"%d\"",
                                         dfg.vertices[m]->meta_routing.src_types[s]);

                }

                json_len += snprintf(json_string + json_len, 2, "]");
            }

            //destinations
            if (strncmp(dfg.vertices[m]->vertex_type, "exit", strlen("entry\0")) != 0 &&
                dfg.vertices[m]->meta_routing.num_dst_types > 0) {

                if (has_sources == 1) {
                    json_len += snprintf(json_string + json_len, 3, ", ");
                }

                field_len = 13 + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"dst_types\":[");

                int s;
                for (s = 0; s < dfg.vertices[m]->meta_routing.num_dst_types; ++s) {
                    if (s > 0) {
                        json_len += snprintf(json_string + json_len, 3, ", ");
                    }

                    //id
                    field_len = 2 + how_many_digits(dfg.vertices[m]->meta_routing.dst_types[s]) + 1;
                    json_len += snprintf(json_string + json_len,
                                         field_len,
                                         "\"%d\"",
                                         dfg.vertices[m]->meta_routing.dst_types[s]);

                }

                json_len += snprintf(json_string + json_len, 2, "]");
            }

            json_len += snprintf(json_string + json_len, 2, "}");

            //scheduling
            json_len += snprintf(json_string + json_len, 3, ", ");
            field_len = 14 + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"scheduling\":{");

            field_len = 15 + how_many_digits(dfg.vertices[m]->scheduling.runtime->id) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"runtime_id\":\"%d\"",
                                 dfg.vertices[m]->scheduling.runtime->id);
            json_len += snprintf(json_string + json_len, 3, ", ");

            field_len = 14 + how_many_digits(dfg.vertices[m]->scheduling.thread_id) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"thread_id\":\"%d\"",
                                 dfg.vertices[m]->scheduling.thread_id);
            json_len += snprintf(json_string + json_len, 3, ", ");

            field_len = 13 + how_many_digits(dfg.vertices[m]->scheduling.deadline) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"deadline\":\"%d\"",
                                 dfg.vertices[m]->scheduling.deadline);

            // Routing
            if (dfg.vertices[m]->scheduling.num_routes > 0) {
                json_len += snprintf(json_string + json_len, 3, ", ");

                field_len = 12 + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"routing\": [");
                int d;
                for (d = 0; d < dfg.vertices[m]->scheduling.num_routes; ++d) {
                    if (d > 0) {
                        json_len += snprintf(json_string + json_len, 3, ", ");
                    }

                    int msu_id =
                            dfg.vertices[m]->scheduling.routes[d]->route_id;

                    field_len = 2 + how_many_digits(msu_id) + 1;
                    json_len += snprintf(json_string + json_len,
                                         field_len,
                                         "\"%d\"",
                                         msu_id);
                }

                json_len += snprintf(json_string + json_len, 2, "]");
            }


            json_len += snprintf(json_string + json_len, 2, "}");

            //statistics
            json_len += snprintf(json_string + json_len, 3, ", ");
            field_len = 14 + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"statistics\":{");

            json_len += snprintf(json_string + json_len, 2, "}");


            json_len += snprintf(json_string + json_len, 2, "}");
        }

        json_len += snprintf(json_string + json_len, 2, "]");
    }


    json_len += snprintf(json_string + json_len, 2, "}\0");


    printf("%s\n", json_string);
}





/**
 * Print a trimmed version of the DFG
 * @return none
 */
void print_dfg(void) {
    struct dfg_config *dfg = get_dfg();
    printf("This DeDOS instance currently manages %d runtimes: \n", dfg->runtimes_cnt);

    if (dfg->runtimes_cnt > 0 ) {
        char ip[INET_ADDRSTRLEN];
        int r;

        for (r = 0; r < dfg->runtimes_cnt; r++) {
            if (dfg->runtimes[r] != NULL) {
                ipv4_to_string(&ip, dfg->runtimes[r]->ip);

                printf("runtime #%d. [ip] %s | [socket] %d | [cores] %d | [pin_t] %d | [threads] %d\n",
                        r + 1,
                        ip,
                        dfg->runtimes[r]->sock,
                        dfg->runtimes[r]->num_cores,
                        dfg->runtimes[r]->num_pinned_threads,
                        dfg->runtimes[r]->num_threads);
            }
        }
    }

    printf("The current dataflow graph has %d MSUs\n", dfg->vertex_cnt);
    if (dfg->vertex_cnt > 0 ) {
        int v = 0;
        for (v = 0; v < dfg->vertex_cnt; v++) {
            if (dfg->vertices[v] != NULL) {
                printf("The %d-th MSU: msu_id=%d, msu_type =%d, msu_mode=%s\n",
                        v + 1,
                        dfg->vertices[v]->msu_id,
                        dfg->vertices[v]->msu_type,
                        dfg->vertices[v]->msu_mode);
            }
        }
    }
}

