#include <string.h>
#include "ip_utils.h"
#include "dfg.h"
#include "jsmn.h"
#include "logging.h"

#define MAX_JSON_LEN 65536

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

#define START_JSON(json) \
    (json).length = 0

#define START_LIST(json) \
    (json).length += sprintf((json).string + (json).length, "[ ")

// This is a hacky trick. Steps back by one to overwrite the previous comma
// If list/obj is empty, it will overwrite the space at the start of the list/obj
#define END_LIST(json) \
    (json).length += sprintf((json).string + (json).length - 1, "],") - 1

#define START_OBJ(json)\
    (json).length += sprintf((json).string + (json).length, "{ " )

#define END_OBJ(json) \
    (json).length += sprintf((json).string + (json).length - 1 , "},") - 1

#define KEY_VAL(json, key, fmt, value) \
    (json).length += sprintf((json).string + (json).length, "\"" key "\":\"" fmt "\",", value)

#define FMT_KEY_VAL(json, key_fmt, key, val_fmt, value) \
    (json).length += sprintf((json).string + (json).length, "\"" key_fmt "\":\"" val_fmt "\",", key, value)

#define KEY(json, key)\
    (json).length += sprintf((json).string + (json).length, "\"" key "\":")

#define VALUE(json, fmt, value) \
    (json).length += sprintf((json).string + (json).length, "\"" fmt "\",", value)

#define END_JSON(json) \
    (json).string[(json).length-1] = '\0'

struct json_output_state {
    char string[MAX_JSON_LEN];
    int list_cnt;
    int length;
};

static struct json_output_state json;

#ifndef LOG_DFG_WRITER
#define LOG_DFG_WRITER 0
#endif

static inline void output_statistic(struct json_output_state *json,
                                    struct timed_rrdb *stat,
                                    int n_statistics) {
    int write_index = stat->write_index;
    START_OBJ(*json);
    KEY(*json, "timestamps");
    START_LIST(*json);
    int skip = 0;
    for (int i=write_index - n_statistics; i<write_index; ++i) {
        int index = i >= 0 ? i : RRDB_ENTRIES + i;
        if (stat->time[index].tv_sec == 0) {
            ++skip;
            continue;
        }
        VALUE(*json, "%.2f", (double)stat->time[index].tv_sec + (double)stat->time[index].tv_nsec / 1e9);
    }
    END_LIST(*json);
    KEY(*json, "values");
    START_LIST(*json);
    for (int i=write_index - n_statistics + skip; i<write_index; ++i) {
        int index = i >= 0 ? i : RRDB_ENTRIES + i;
        VALUE(*json, "%.3f", stat->data[index]);
    }
    END_LIST(*json);
    END_OBJ(*json);
}

/**
 * Parse the DFG and format it as a JSON string
 * @return none
 */
char *dfg_to_json(struct dfg_config *dfg, int n_statistics) {

    START_JSON(json);

    START_OBJ(json);

    /**
     *  APPLICATION
     */

    //application name
    KEY_VAL(json, "application_name", "%s", dfg->application_name);

    //applicatin deadline
    KEY_VAL(json, "application_deadline", "%d", (int)dfg->application_deadline);

    //global ctl ip
    KEY_VAL(json, "global_ctl_ip", "%s", dfg->global_ctl_ip);

    //global ctl port
    KEY_VAL(json, "global_ctl_port", "%d", dfg->global_ctl_port);

    /**
     *  RUNTIMES
     */

    if (dfg->runtimes_cnt > 0 ){

        KEY(json, "runtimes");
        START_LIST(json);

        for (int r=0; r<dfg->runtimes_cnt; ++r) {
            log_custom(LOG_DFG_WRITER, "writing runtime %d", r);
            struct dfg_runtime_endpoint *rt = dfg->runtimes[r];

            START_OBJ(json);

            KEY_VAL(json, "id", "%d", rt->id);
            if ( rt->sock != -1 ) {
                KEY_VAL(json, "sock", "%d", rt->sock);
            }
            KEY(json, "routes");

            START_LIST(json);

            for (int route_i = 0; route_i < rt->num_routes; ++route_i) {
                struct dfg_route *route = rt->routes[route_i];

                START_OBJ(json);
                KEY_VAL(json, "id", "%d", route->route_id);

                KEY(json, "destinations");
                START_OBJ(json);

                for (int dest_i = 0; dest_i < route->num_destinations; ++dest_i) {
                    FMT_KEY_VAL(json, "%d", route->destinations[dest_i]->msu_id,
                                      "%d", route->destination_keys[dest_i]);
                }

                END_OBJ(json);
                END_OBJ(json);
            }
            END_LIST(json);

            struct in_addr addr;
            addr.s_addr = rt->ip;
            char *ipstr = inet_ntoa(addr);
            log_custom(LOG_DFG_WRITER, "Got IP string: %s", ipstr);
            if ( ipstr != NULL ) {
                KEY_VAL(json, "ip", "%s", ipstr);
            } else {
                log_error("Could not convert IP to string");
            }

            // port
            KEY_VAL(json, "port", "%d", rt->port);
            // Number of cores
            KEY_VAL(json, "num_cores", "%d", rt->num_cores);
            // dram
            KEY_VAL(json, "dram", "%ld", rt->dram);
            // io_network_bw
            KEY_VAL(json, "io_network_bw", "%ld", rt->io_network_bw);

            if (rt->current_alloc != NULL) {
                int timestamp = time(NULL);

                START_OBJ(json);
                KEY_VAL(json, "timestamp", "%d", timestamp);
                // non-blocking-threads
                KEY_VAL(json, "non_blocking_workers", "%d", rt->num_pinned_threads);
                // blocking threads
                int num_blocking = rt->num_threads - rt->num_pinned_threads;
                KEY_VAL(json, "blocking_workers", "%d", num_blocking);
                // cores
                KEY_VAL(json, "cores", "%d",  rt->current_alloc->num_cores);
                // bissection bw
                KEY_VAL(json, "io_network_bw", "%ld", rt->current_alloc->io_network_bw);
                // egress bw
                KEY_VAL(json, "egress_bw", "%ld", rt->current_alloc->egress_bw);
                // Incress bw
                KEY_VAL(json, "ingress_bw", "%ld", rt->current_alloc->ingress_bw);
                // num msus
                KEY_VAL(json, "num_msus", "%d", rt->current_alloc->num_msu);

                END_OBJ(json);
            }

            END_OBJ(json);

        }

        END_LIST(json);
    }

    /**
     *  MSUS
     */

    if (dfg->vertex_cnt > 0 ) {
        log_custom(LOG_DFG_WRITER, "Writing %d MSUs", dfg->vertex_cnt);
        KEY(json, "MSUs");
        START_LIST(json);

        for (int msu_i=0; msu_i < dfg->vertex_cnt; ++msu_i) {
            log_custom(LOG_DFG_WRITER, "Writing MSU %d", msu_i);
            struct dfg_vertex *msu = dfg->vertices[msu_i];
            START_OBJ(json);
            // id
            KEY_VAL(json, "id", "%d", msu->msu_id);
            // name
            KEY_VAL(json, "name", "%s", msu->msu_name);
            // type
            KEY_VAL(json, "type", "%d", msu->msu_type);
            // mode
            KEY_VAL(json, "working-mode", "%s", msu->msu_mode);
            // profiling
            KEY(json, "profiling");
            START_OBJ(json);

            log_custom(LOG_DFG_WRITER, "Writing Profiling");
            // wcet
            KEY_VAL(json, "wcet", "%d", msu->profiling.wcet);
            // footprint
            KEY_VAL(json, "mem_footprint", "%ld", msu->profiling.dram);
            // tx_node_local
            KEY_VAL(json, "tx_node_local", "%d", msu->profiling.tx_node_local);
            // tx_core_local
            KEY_VAL(json, "tx_core_local", "%d", msu->profiling.tx_core_local);
            // tx_node_remote
            KEY_VAL(json, "tx_node_remote", "%d", msu->profiling.tx_node_remote);
            END_OBJ(json);

            KEY(json, "meta_routing");
            START_OBJ(json);

            // sources
            if (strcmp(msu->vertex_type, "entry") != 0 &&
                    msu->meta_routing.num_src_types > 0 ) {
                KEY(json, "sources_types");
                START_LIST(json);
                for (int s=0; s < msu->meta_routing.num_src_types; ++s) {
                    VALUE(json, "%d", msu->meta_routing.src_types[s]);
                }
                END_LIST(json);
            }

            // destinations
            if (strcmp(msu->vertex_type, "exit") != 0 &&
                    msu->meta_routing.num_dst_types > 0) {

                KEY(json, "dst_types");
                START_LIST(json);
                for (int s=0; s<msu->meta_routing.num_dst_types; ++s) {
                    VALUE(json, "%d", msu->meta_routing.dst_types[s]);
                }
                END_LIST(json);
            }

            END_OBJ(json);

            log_custom(LOG_DFG_WRITER, "writing scheduling");
            KEY(json, "scheduling");
            START_OBJ(json);

            KEY_VAL(json, "runtime_id", "%d", msu->scheduling.runtime->id);
            KEY_VAL(json, "thread_id", "%d", msu->scheduling.thread_id);
            KEY_VAL(json, "deadline", "%d", (int)msu->scheduling.deadline);

            log_custom(LOG_DFG_WRITER, "writing routes");
            if (msu->scheduling.num_routes > 0) {
                KEY(json, "routing");
                START_LIST(json);

                for (int r=0; r<msu->scheduling.num_routes; ++r) {
                    VALUE(json, "%d", msu->scheduling.routes[r]->route_id);
                }
                END_LIST(json);
            }

            END_OBJ(json);


            KEY(json, "statistics");
            START_OBJ(json);

            KEY(json, "queue_items_processed");
            output_statistic(&json, &msu->statistics.queue_items_processed, n_statistics);
            KEY(json, "queue_length");
            output_statistic(&json, &msu->statistics.queue_length, n_statistics);
            KEY(json, "memory_allocated");
            output_statistic(&json, &msu->statistics.memory_allocated, n_statistics);
            // END statistics
            END_OBJ(json);

            // END MSU
            END_OBJ(json);
        }
        // END MSUs
        END_LIST(json);
    }

    // END top-level object
    END_OBJ(json);

    END_JSON(json);
    return json.string;
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
                ipv4_to_string(ip, dfg->runtimes[r]->ip);

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


void dfg_to_file(struct dfg_config *dfg, int n_statistics, char *filename) {
    char *dfg_json = dfg_to_json(dfg, n_statistics);
    int json_size = strlen(dfg_json);
    FILE *file = fopen(filename, "w");
    fwrite(dfg_json, sizeof(char), json_size, file);
    fclose(file);
    log_custom(LOG_DFG_WRITER, "Wrote DFG with length %d to %s", json_size, filename);
}


