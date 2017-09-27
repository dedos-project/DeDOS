#include <string.h>
#include <stdlib.h>
#include "msu_stats.h"
#include "timeseries.h"
#include "dfg.h"
#include "jsmn.h"
#include "logging.h"

#define JSON_LEN_INCREMENT 1024

#define CHECK_JSON_LEN(json, len)\
    if ( (json).allocated_size - (json).length < len) { \
        log(LOG_DFG_WRITER, "Reallocating to %d", (int)(json.allocated_size + JSON_LEN_INCREMENT)); \
        (json).string = realloc((json).string, (json).allocated_size + JSON_LEN_INCREMENT); \
        (json).allocated_size += JSON_LEN_INCREMENT; \
    }

#define START_JSON(json) \
    (json).length = 0

#define START_LIST(json) \
    do { \
        CHECK_JSON_LEN(json, 2); \
        (json).length += sprintf((json).string + (json).length, "[ "); \
    } while (0)


// This is a hacky trick. Steps back by one to overwrite the previous comma
// If list/obj is empty, it will overwrite the space at the start of the list/obj
#define END_LIST(json) \
    do { \
        CHECK_JSON_LEN(json, 2); \
        (json).length += sprintf((json).string + (json).length - 1, "],") - 1; \
    } while (0)

#define START_OBJ(json)\
    do { \
        CHECK_JSON_LEN(json, 2); \
        (json).length += sprintf((json).string + (json).length, "{ " ); \
    } while (0)

#define END_OBJ(json) \
    do { \
        CHECK_JSON_LEN(json, 2); \
        (json).length += sprintf((json).string + (json).length - 1 , "},") - 1; \
    } while (0)

#define KEY_VAL(json, key, fmt, value, value_len) \
    do { \
        CHECK_JSON_LEN(json, value_len + strlen(key) + 8); \
        (json).length += sprintf((json).string + (json).length, "\"" key "\":" fmt ",", value); \
    } while (0)

#define KEY_INTVAL(json, key, value) \
    KEY_VAL(json, key, "%d", value, 128)

#define KEY_STRVAL(json, key, value) \
    KEY_VAL(json, key, "\"%s\"", value, strlen(value))

#define FMT_KEY_VAL(json, key_fmt, key, key_len, val_fmt, value, value_len) \
    do { \
        CHECK_JSON_LEN(json, key_len + value_len + 8); \
        (json).length += sprintf((json).string + (json).length, "\"" key_fmt "\":\"" val_fmt "\",", key, value); \
    } while (0)

#define KEY(json, key)\
    do { \
        CHECK_JSON_LEN(json, strlen(key) + 4); \
        (json).length += sprintf((json).string + (json).length, "\"" key "\":"); \
    } while (0)

#define VALUE(json, fmt, value, value_len) \
    do { \
        CHECK_JSON_LEN(json, value_len + 4); \
        (json).length += sprintf((json).string + (json).length,  fmt ",", value); \
    } while (0)

#define END_JSON(json) \
    (json).string[(json).length-1] = '\0'

struct json_output {
    char *string;
    size_t allocated_size;
    int list_cnt;
    int length;
};


char *stat_to_json(struct timed_rrdb *timeseries, int n_stats) {
    static struct json_output json;

    int write_index = timeseries->write_index;
    START_JSON(json);
    START_LIST(json);

    for (int i= write_index - n_stats - 1; i<write_index; i++) {
        int index = i >= 0 ? i : RRDB_ENTRIES + i;
        if (timeseries->time[index].tv_sec == 0 && timeseries->time[index].tv_nsec == 0) {
            continue;
        }
        START_OBJ(json);
        KEY_VAL(json, "time", "%.2f", (double)timeseries->time[index].tv_sec +
                                      (double)timeseries->time[index].tv_nsec / 1e9,
                16);
        KEY_VAL(json, "value", "%.3f", timeseries->data[index], 16);
        END_OBJ(json);
    }
    END_LIST(json);
    END_JSON(json);
    return json.string;
}


char *meta_routing_to_json(struct dfg_meta_routing *meta_routing) {
    static struct json_output json;

    START_JSON(json);
    START_OBJ(json);

    KEY(json, "src_types");
    START_LIST(json);
    for (int i=0; i<meta_routing->n_src_types; i++) {
        VALUE(json, "%d", meta_routing->src_types[i]->id, 8);
    }
    END_LIST(json);

    KEY(json, "dst_types");
    START_LIST(json);
    for (int i=0; i<meta_routing->n_dst_types; i++) {
        VALUE(json, "%d", meta_routing->dst_types[i]->id, 8);
    }
    END_LIST(json);

    END_OBJ(json);
    END_JSON(json);
    return json.string;
}

char *dependency_to_json(struct dfg_dependency *dep) {
    static struct json_output json;

    START_JSON(json);
    START_OBJ(json);
    KEY_INTVAL(json, "type", dep->type->id);
    KEY_STRVAL(json, "locality", dep->locality == MSU_IS_LOCAL ? "local" : "remote");
    END_OBJ(json);
    END_JSON(json);

    return json.string;
}

char *msu_type_to_json(struct dfg_msu_type *type) {
    static struct json_output json;

    START_JSON(json);
    START_OBJ(json);

    KEY_INTVAL(json, "id", type->id);
    KEY_STRVAL(json, "name", type->name);

    char *meta_routing = meta_routing_to_json(&type->meta_routing);
    KEY(json, "meta_routing");
    VALUE(json, "%s", meta_routing, strlen(meta_routing));

    KEY(json, "dependencies");
    START_LIST(json);
    for (int i=0; i<type->n_dependencies; i++) {
        char *dependency = dependency_to_json(type->dependencies[i]);
        VALUE(json, "%s", dependency, strlen(dependency));
    }
    END_LIST(json);

    KEY_INTVAL(json, "cloneable", type->cloneable);
    KEY_INTVAL(json, "colocation_group", type->colocation_group);

    END_OBJ(json);
    END_JSON(json);
    return json.string;
}

char *scheduling_to_json(struct dfg_scheduling *sched) {
    static struct json_output json;

    START_JSON(json);
    START_OBJ(json);

    KEY_INTVAL(json, "runtime", sched->runtime->id);
    KEY_INTVAL(json, "thread", sched->thread->id);

    KEY(json, "routes");
    START_LIST(json);
    for (int i=0; i<sched->n_routes; i++) {
        VALUE(json, "%d", sched->routes[i]->id, 8);
    }
    END_LIST(json);

    END_OBJ(json);
    END_JSON(json);

    return json.string;
}

char *msu_stats_to_json(int msu_id, int n_stats) {
    static struct json_output json;

    START_JSON(json);
    START_LIST(json);

    for (int i=0; i<N_REPORTED_STAT_TYPES; i++) {
        START_OBJ(json);
        KEY_STRVAL(json, "label", reported_stat_types[i].name);
        KEY(json, "stats");
        struct timed_rrdb *stat = get_stat(reported_stat_types[i].id, msu_id);
        char *stat_json = stat_to_json(stat, n_stats);
        VALUE(json, "%s", stat_json, strlen(stat_json));
        END_OBJ(json);
    }
    END_LIST(json);
    END_JSON(json);

    return json.string;
}


char *msu_to_json(struct dfg_msu *msu, int n_stats) {
    static struct json_output json;

    START_JSON(json);
    START_OBJ(json);

    KEY_INTVAL(json, "id", msu->id);
    if (msu->vertex_type & ENTRY_VERTEX_TYPE && msu->vertex_type & EXIT_VERTEX_TYPE) {
        KEY_STRVAL(json, "vertex_type", "entry/exit");
    } else if (msu->vertex_type & ENTRY_VERTEX_TYPE) {
        KEY_STRVAL(json, "vertex_type", "entry");
    } else if (msu->vertex_type & EXIT_VERTEX_TYPE) {
        KEY_STRVAL(json, "vertex_type", "exit");
    } else {
        KEY_STRVAL(json, "vertex_type", "nop");
    }

    KEY_STRVAL(json, "init_data", msu->init_data.init_data);
    KEY_INTVAL(json, "type", msu->type->id);
    KEY_STRVAL(json, "blocking_mode",
               msu->blocking_mode == BLOCKING_MSU ? "blocking" : "non-blocking");

    char *scheduling = scheduling_to_json(&msu->scheduling);
    KEY_VAL(json, "scheduling", "%s", scheduling, strlen(scheduling));

    if (n_stats > 0) {
        char *stats = msu_stats_to_json(msu->id, n_stats);
        KEY_VAL(json, "stats", "%s", stats, strlen(stats));
    }


    END_OBJ(json);
    END_JSON(json);
    return json.string;
}

char *endpoint_to_json(struct dfg_route_endpoint *ep) {
    static struct json_output json;
    START_JSON(json);
    START_OBJ(json);

    KEY_INTVAL(json, "key", ep->key);
    KEY_INTVAL(json, "msu", ep->msu->id);

    END_OBJ(json);
    END_JSON(json);
    return json.string;
}

char *route_to_json(struct dfg_route *route) {
    static struct json_output json;

    START_JSON(json);
    START_OBJ(json);

    KEY_INTVAL(json, "id", route->id);
    KEY_INTVAL(json, "type", route->msu_type->id);

    KEY(json, "endpoints");
    START_LIST(json);
    for (int i=0; i<route->n_endpoints; i++) {
        char *ep = endpoint_to_json(route->endpoints[i]);
        VALUE(json, "%s", ep, strlen(ep));
    }
    END_LIST(json);
    END_OBJ(json);
    END_JSON(json);

    return json.string;
}


char *runtime_to_json(struct dfg_runtime *rt) {
    static struct json_output json;

    START_JSON(json);
    START_OBJ(json);

    KEY_INTVAL(json, "id", rt->id);
    char ip[32];
    struct in_addr addr = {rt->ip};
    inet_ntop(AF_INET, &addr, ip, 32);
    KEY_STRVAL(json, "ip", ip);

    KEY_INTVAL(json, "port", rt->port);
    KEY_INTVAL(json, "n_cores", rt->n_cores);

    KEY_INTVAL(json, "n_pinned_threads", rt->n_pinned_threads);
    KEY_INTVAL(json, "n_unpinned_threads", rt->n_unpinned_threads);

    KEY(json, "routes");
    START_LIST(json);
    for (int i=0; i<rt->n_routes; i++) {
        char *route = route_to_json(rt->routes[i]);
        VALUE(json, "%s", route, strlen(route));
    }
    END_LIST(json);
    END_OBJ(json);
    END_JSON(json);

    return json.string;
}

char *dfg_to_json(struct dedos_dfg *dfg, int n_stats) {
    static struct json_output json;

    START_JSON(json);
    START_OBJ(json);

    KEY_STRVAL(json, "application_name", dfg->application_name);
    char ip[32];
    struct in_addr addr = {htons(dfg->global_ctl_ip)};
    inet_ntop(AF_INET, &addr, ip, 32);
    log(LOG_TEST, "IP IS %s", ip);
    KEY_STRVAL(json, "global_ctl_ip", ip);

    KEY_INTVAL(json, "global_ctl_port", dfg->global_ctl_port);

    KEY(json, "msu_types");
    START_LIST(json);
    for (int i=0; i<dfg->n_msu_types; i++) {
        char *type = msu_type_to_json(dfg->msu_types[i]);
        VALUE(json, "%s", type, strlen(type));
    }
    END_LIST(json);

    KEY(json, "msus");
    START_LIST(json);
    for (int i=0; i<dfg->n_msus; i++) {
        char *msu = msu_to_json(dfg->msus[i], n_stats);
        VALUE(json, "%s", msu, strlen(msu));
    }
    END_LIST(json);

    KEY(json, "runtimes");
    START_LIST(json);
    for (int i=0; i<dfg->n_runtimes; i++) {
        char *rt = runtime_to_json(dfg->runtimes[i]);
        VALUE(json, "%s", rt, strlen(rt));
    }
    END_LIST(json);

    END_OBJ(json);
    END_JSON(json);

    return json.string;
}


/**
 * Parse the DFG and format it as a JSON string
 * @return none
char *dfg_to_json(struct dfg_config *dfg, int n_statistics) {
    static struct json_output_state json;

    START_JSON(json);

    START_OBJ(json);

    ////// APPLICATION

    //application name
    KEY_VAL(json, "application_name", "%s", dfg->application_name);

    //applicatin deadline
    KEY_VAL(json, "application_deadline", "%d", (int)dfg->application_deadline);

    //global ctl ip
    KEY_VAL(json, "global_ctl_ip", "%s", dfg->global_ctl_ip);

    //global ctl port
    KEY_VAL(json, "global_ctl_port", "%d", dfg->global_ctl_port);

    ///// RUNTIMES

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

    ////////  MSUS

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
*/





/**
 * Print a trimmed version of the DFG
 * @return none
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
 */

/*
void dfg_to_file(struct dfg_config *dfg, int n_statistics, char *filename) {
    char *dfg_json = dfg_to_json(dfg, n_statistics);
    int json_size = strlen(dfg_json);
    FILE *file = fopen(filename, "w");
    fwrite(dfg_json, sizeof(char), json_size, file);
    fclose(file);
    log_custom(LOG_DFG_WRITER, "Wrote DFG with length %d to %s", json_size, filename);
}
*/


