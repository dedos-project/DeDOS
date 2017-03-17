#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "dfg.h"
#include "logging.h"
#include "control_protocol.h"
#include "controller_tools.h"
#include "jsmn.h"
#include "scheduling.h"


/**
 * This section of the file defines JSON related functions
 */

jsmn_parser jp;
struct dfg_config dfg;    //The global config for DFG.

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
    strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

static void json_parse_error(int err) {
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

static int json_test(jsmntok_t *t, int r) {
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

static int parse_msu_profiling(jsmntok_t *t, int starting_token,
                               const char *json_string, struct dfg_vertex *v) {
    int i = starting_token;

    if (t[i].type != JSMN_OBJECT) {
        debug("DEBUG: Expected %d-th field to be JSMN_OBJECT but found %d",
              i, t[i].type);
        return -1;
    }

    int fields = t[i].size;

    struct msu_profiling_data *p = NULL;
    p = malloc(sizeof(struct msu_profiling_data));

    v->profiling_data = p;

    int s;
    for (s = 1; s < fields * 2; s += 2) {
        if (t[i+s].type != JSMN_STRING || t[i+s+1].type != JSMN_STRING) {
            debug("DEBUG: Expected JSMN_STRING but found %d\n", t[i+s].type);
            return -1;
        }
        debug("String name : %.*s\n", t[i+s].end-t[i+s].start, json_string + t[i+s].start);

        char value[32];
        memset(value, '\0', 32);
        memcpy(value, json_string + t[i+s+1].start, t[i+s+1].end - t[i+s+1].start);

        if (jsoneq(json_string, &t[i+s], "wcet") == 0) {
            debug("'wcet' field. Value: %s\n", value);

            v->profiling_data->wcet = atoi(value);

        } else if (jsoneq(json_string, &t[i+s], "dram_footprint") == 0) {
            debug("'dram_footprint' field. Value: %s\n", value);

            v->profiling_data->dram_footprint = atol(value);

        } else if (jsoneq(json_string, &t[i+s], "tx_node_local") == 0) {
            debug("'tx_node_local' field. Value: %s\n", value);

            v->profiling_data->tx_node_local = atoi(value);

        } else if (jsoneq(json_string, &t[i+s], "tx_core_local") == 0) {
            debug("'tx_core_local' field. Value: %s\n", value);

            v->profiling_data->tx_core_local = atoi(value);

        } else if (jsoneq(json_string, &t[i+s], "tx_node_remote") == 0) {
            debug("'tx_node_remote' field. Value: %s\n", value);

            v->profiling_data->tx_node_remote = atoi(value);

        } else {
            fprintf(stdout, "unknown key: %*.*s",
                    t[i+s].end - t[i+s].start, json_string + t[i+s].start);
            return -1;
        }
    }

    return 0;
}

static int json_parse_runtimes(jsmntok_t *t, int starting_token,
                               int ending_token, const char *json_string) {
    int i = starting_token;

    if (t[i].type != JSMN_ARRAY) {
        debug("DEBUG: Expected %d-th field to be JSMN_ARRAY but found %d",
              i, t[i].type);
        return -1;
    }

    //jump to the first runtime object
    i++;

    struct runtime_endpoint *r = NULL;
    while (i < ending_token) {
        if (t[i].type != JSMN_OBJECT) {
            debug("DEBUG: At location %d, expected JSMN_OBJECT but found %d\n", i, t[i].type);
            return -1;
        }

        int fields = t[i].size;
        struct runtime_endpoint *r = NULL;
        r = malloc(sizeof(struct runtime_endpoint));

        dfg.runtimes[dfg.runtimes_cnt] = r;
        dfg.runtimes_cnt++;

        //init socket at -1
        r->sock = -1;

        int s;
        for (s = 1; s <= fields * 2; s += 2) {
            // field name should be a JSMN_STRING
            if (t[i+s].type != JSMN_STRING) {
                debug("DEBUG: Expected JSMN_STRING but found %d\n", t[i+s].type);
                return -1;
            }
            debug("String name : %.*s\n", t[i+s].end-t[i+s].start, json_string + t[i+s].start);

            char value[32];
            memset(value, '\0', 32);
            memcpy(value, json_string + t[i+s+1].start, t[i+s+1].end - t[i+s+1].start);

            if (jsoneq(json_string, &t[i+s], "id") == 0) {
                debug("'id' field. Value: %s\n", value);

                r->id = atoi(value);

            } else if (jsoneq(json_string, &t[i+s], "ip") == 0) {
                debug("'ip' field. Value: %s\n", value);

                string_to_ipv4(value, &(r->ip));

            } else if (jsoneq(json_string, &t[i+s], "port") == 0) {
                debug("'port' field. Value: %s\n", value);

                r->port = atoi(value);

            } else if (jsoneq(json_string, &t[i+s], "num_cores") == 0) {
                debug("'num_cores' field. Value: %s\n", value);

                r->num_cores = atoi(value);

            } else if (jsoneq(json_string, &t[i+s], "dram") == 0) {
                debug("'num_cores' field. Value: %s\n", value);

                r->dram = atol(value);

            } else if (jsoneq(json_string, &t[i+s], "io_network_bw") == 0) {
                debug("'num_cores' field. Value: %s\n", value);

                r->io_network_bw = atol(value);

            } else {
                fprintf(stdout, "unknown key: %*.*s",
                        t[i+s+1].end - t[i+s+1].start, json_string + t[i+s+1].start);
                return -1;
            }
        }

        //go to last object's field
        i += fields * 2;
        //jump to next object
        i++;
    }

    return 0;
}

static int json_parse_msu(jsmntok_t *t, int starting_token, int msu_cnt, const char *json_string) {
    int i = starting_token;
    int c = 0;

    if (t[i].type != JSMN_ARRAY) {
        debug("DEBUG: Expected %d-th field to be JSMN_ARRAY but found %d",
              i, t[i].type);
        return -1;
    }

    // Jump to the first msu object in the array
    i++;

    //Prepare the MSU struct:
    //Parse msu objects in the array one by one.
    while (c < msu_cnt) {
        if (t[i].type != JSMN_OBJECT) {
            debug("DEBUG: At location %d, expected JSMN_OBJECT but found %d\n", i, t[i].type);
            return -1;
        }

        //number of MSU fields
        int fields = t[i].size;
        int f = 0;;

        //Create a new MSU:
        struct dfg_vertex *v = NULL;
        v = (struct dfg_vertex*)calloc(1, sizeof(struct dfg_vertex));
        dfg.vertices[dfg.vertex_cnt] = v;
        dfg.vertex_cnt++;

        //init some base fields
        v->num_dst = 0;
        v->num_src = 0;
        v->scheduling = NULL;

        //jump in the first field of the MSU object
        i++;

        // for each field we increment the base index, starting from jsmn OBJECT, and define
        // an offset s
        while (f < fields) {
            // field name should be a JSMN_STRING
            if (t[i].type != JSMN_STRING) {
                debug("DEBUG: Expected JSMN_STRING but found %d\n", t[i].type);
                return -1;
            }
            debug("Field name: %.*s\n", t[i].end-t[i].start, json_string + t[i].start);

            if (jsoneq(json_string, &t[i], "id") == 0) {
                debug("'id' field. Value: %.*s\n\n",
                      t[i+1].end - t[i+1].start, json_string + t[i+1].start);

                if (t[i+1].type != JSMN_STRING) {
                    debug("DEBUG: Expected JSMN_STRING but found %d\n", t[i+1].type);
                    return -1;
                }

                char id[4];
                memset(id, '\0', 4);
                memcpy(id, json_string + t[i+1].start, t[i+1].end - t[i+1].start);

                v->msu_id = atoi(id);

                f++;
                i += 2;
            } else if (jsoneq(json_string, &t[i], "type") == 0) {
                debug("'type' field. Value: %.*s\n\n",
                      t[i+1].end - t[i+1].start, json_string + t[i+1].start);

                if (t[i+1].type != JSMN_STRING) {
                    debug("DEBUG: Expected JSMN_STRING but found %d\n", t[i+1].type);
                    return -1;
                }

                char type[5];
                memset(type, '\0', 5);
                memcpy(type, json_string + t[i+1].start, t[i+1].end - t[i+1].start);

                v->msu_type = atoi(type);

                f++;
                i += 2;
            } else if (jsoneq(json_string, &t[i], "working_mode") == 0) {
                debug("'working_mode' field. Value: %.*s\n\n",
                      t[i+1].end - t[i+1].start, json_string + t[i+1].start);

                if (t[i+1].type != JSMN_STRING) {
                    debug("DEBUG: Expected JSMN_STRING but found %d\n", t[i+1].type);
                    return -1;
                }

                memcpy(v->msu_mode, json_string + t[i+1].start, t[i+1].end - t[i+1].start);

                f++;
                i += 2;
            } else if (jsoneq(json_string, &t[i], "profiling") == 0) {
                debug("'profiling' field. Value: %.*s\n\n",
                      t[i+1].end - t[i+1].start, json_string + t[i+1].start);

                if (t[i+1].type != JSMN_OBJECT) {
                    debug("DEBUG: Expected JSMN_OBJECT but found %d\n", t[i+1].type);
                    return -1;
                }

                parse_msu_profiling(t, i+1, json_string, v);

                f++;
                i += 2 + MSU_PROFILING_FIELDS;
            } else if (jsoneq(json_string, &t[i], "meta_routing") == 0) {
                // annoying because dynamic (i.e different MSUs have different routes)
                // we have to determine on the go how much to increment i
                //TODO: handle case where we have no source or destination

                debug("'meta_routing' field. Value: %.*s\n\n",
                      t[i+1].end - t[i+1].start, json_string + t[i+1].start);

                if (t[i+1].type != JSMN_OBJECT) {
                    debug("DEBUG: Expected JSMN_OBJECT but found %d\n", t[i+1].type);
                    return -1;
                }

                struct msu_meta_routing *m = NULL;
                m = malloc(sizeof(struct msu_meta_routing));
                v->meta_routing = m;

                //jump to first item in the meta_routing object
                i += 2;

                int num_src, num_dst;

                if (jsoneq(json_string, &t[i], "source_types") == 0) {
                    debug("'source_types' field. Value: %.*s\n\n",
                          t[i+1].end - t[i+1].start, json_string + t[i+1].start);

                    if (t[i+1].type != JSMN_ARRAY) {
                        debug("DEBUG: Expected JSMN_ARRAY but found %d\n", t[i+1].type);
                        return -1;
                    }

                    v->meta_routing->num_src = 0;
                    num_src = t[i+1].size;

                    //jump to the first element of the array
                    i += 2;

                    int s;
                    char value[4];
                    for (s = 0; s < num_src; s++) {
                        debug("New source MSU: %.*s\n\n",
                          t[i].end - t[i].start, json_string + t[i].start);

                        if (t[i].type != JSMN_STRING) {
                            debug("DEBUG: Expected JSMN_STRING but found %d\n", t[i].type);
                            return -1;
                        }

                        memset(value, '\0', 4);
                        memcpy(value, json_string + t[i].start, t[i].end - t[i].start);
                        v->meta_routing->src_types[v->meta_routing->num_src] = atoi(value);
                        v->meta_routing->num_src++;

                        //jump to next source (last jump will be to dst_types)
                        i++;
                    }
                } else {
                    debug(" expected 'source_types' field. Found: %.*s\n\n",
                        t[i].end - t[i].start, json_string + t[i].start);

                    return -1;
                }

                if (jsoneq(json_string, &t[i], "dst_types") == 0) {
                    debug("'dst_types' field. Value: %.*s\n\n",
                          t[i].end - t[i].start, json_string + t[i].start);

                    if (t[i+1].type != JSMN_ARRAY) {
                        debug("DEBUG: Expected JSMN_ARRAY but found %d\n", t[i+1].type);
                        return -1;
                    }

                    v->meta_routing->num_dst = 0;
                    num_dst = t[i+1].size;

                    //jump to the first element of the array
                    i += 2;

                    int d;
                    char value[4];
                    for (d = 0; d < num_dst; d++) {
                        debug("New dst MSU: %.*s\n\n",
                          t[i].end - t[i].start, json_string + t[i].start);

                        if (t[i].type != JSMN_STRING) {
                            debug("DEBUG: Expected JSMN_STRING but found %d\n", t[i].type);
                            return -1;
                        }

                        memset(value, '\0', 4);
                        memcpy(value, json_string + t[i].start, t[i].end - t[i].start);
                        v->meta_routing->dst_types[v->meta_routing->num_dst] = atoi(value);
                        v->meta_routing->num_dst++;

                        //jump to next destination
                        i++;
                    }
                } else {
                    debug(" expected 'dst_types' field. Found: %.*s\n\n",
                        t[i].end - t[i].start, json_string + t[i].start);

                    return -1;
                }

                f++;
            } else {
                fprintf(stdout, "unknown key: %*.*s",
                        t[i].end - t[i].start, json_string + t[i].start);
            }
        }

        c++;
    }

    return 0;
}

//Read in the dataflow graph
int do_dfg_config(char * init_cfg_filename) {
    if ((strlen(init_cfg_filename) == 0) || (strlen(init_cfg_filename) > 128)) {
        debug("ERROR: %s", "invalid DFG filename");
        return -1;
    }

    /**
     * Init some key elements of the dataflow graph
     */

    pthread_mutex_t m; // Need a mutex to make the dfg thread safe
    dfg.dfg_mutex = &m;
    if (pthread_mutex_init(dfg.dfg_mutex, NULL) != 0) {
        debug("ERROR: %s", "failed to init dfg mutex");
    }

    dfg.vertex_cnt = 0;
    dfg.runtimes_cnt = 0;

    int vertex;
    for (vertex = 0; vertex < MAX_MSU; vertex++) {
        dfg.vertices[vertex] = NULL;
    }

    strncpy(dfg.init_dfg_filename,
            init_cfg_filename,
            strlen(init_cfg_filename));
    debug("DEBUG: Loading dataflow graph from : %s", dfg.init_dfg_filename);

    /**
     * Read JSON file
     */

    FILE *fin = fopen(dfg.init_dfg_filename, "r");
    if (!fin) {
        debug("DEBUG: file open error: %s", dfg.init_dfg_filename);
        return -1;
    } else {
        debug("DEBUG: opened JSON file: %s", dfg.init_dfg_filename);
    }

    const char json_string[MAX_JSON_LEN]; //file content
    int linecnt = 0;
    while (fgets(json_string, sizeof(json_string), fin) != NULL) {
        debug("DEBUG: JSON string: %s", json_string);
        linecnt ++;
    }

    if (linecnt != 1) {
        debug("%s", "Malformed JSON file, one-line file expected.");
        return -1;
    }

    /*Do JSON parsing*/
    jsmntok_t t[1024];//num. of JSON tokens.
    jsmn_init(&jp);
    int r = jsmn_parse(&jp, json_string, strlen(json_string), t, sizeof(t)/sizeof(t[0]));
    if (r < 0) {
        json_parse_error(r);
        return -1;
    }

    int ret;
    if ((ret = json_test(t, r)) != 0) {
        debug("%s", "malformed JSON");
        return -1;
    }

    //The actual pass
    int i = 1;

    if (jsoneq(json_string, &t[i], "application_name") != 0) {
        debug("DEBUG: Expected 'application_name' but was %.*s\n",
              t[i].end - t[i].start, json_string+t[i].start);
        return -1;
    }
    strncpy(dfg.application_name, json_string + t[i+1].start, t[i+1].end - t[i+1].start);
    i += 2;

    if (jsoneq(json_string, &t[i], "global_ctl_ip") != 0) {
        debug("DEBUG: Expected 'global_ctl_ip' but was %.*s\n",
              t[i].end - t[i].start, json_string+t[i].start);
        return -1;
    }
    strncpy(dfg.global_ctl_ip, json_string + t[i+1].start, t[i+1].end - t[i+1].start);
    i += 2;

    if (jsoneq(json_string, &t[i], "global_ctl_port") != 0) {
        debug("DEBUG: Expected 'global_ctl_port' but was %.*s\n",
              t[i].end - t[i].start, json_string+t[i].start);
        return -1;
    }
    char port[5];
    strncpy(port, json_string + t[i+1].start, t[i+1].end - t[i+1].start);
    dfg.global_ctl_port = atoi(port);
    i += 2;

    if (jsoneq(json_string, &t[i], "load_mode") != 0) {
        debug("DEBUG: Expected 'load_mode' but was %.*s\n",
              t[i].end - t[i].start, json_string+t[i].start);
        return -1;
    }

    char load_mode[8];
    strncpy(load_mode, json_string + t[i + 1].start, t[i + 1].end - t[i + 1].start);

    printf("load mode: %s\n", load_mode);

    i += 2;

    //runtimes
    if (jsoneq(json_string, &t[i], "runtimes") != 0) {
        debug("DEBUG: Expected 'runtimes' but was %.*s\n",
              t[i].end - t[i].start, json_string+t[i].start);
        return -1;
    }
    i++;
    int runtimes_cnt = t[i].size;
    int runtimes_tokens = runtimes_cnt + (RUNTIME_FIELDS * 2 * runtimes_cnt);

    debug("DEBUG: %d runtimes have been declared.", runtimes_cnt);

    //ending idx = current idx + 1 (json array) + #runtimes (json objects) + #msu * #runtime_fields
    ret = json_parse_runtimes(t, i, i + 1 + runtimes_tokens, json_string);
    if (ret != 0) {
        debug("ERROR: %s", "could not extract runtimes from JSON");
        return -1;
    }

    i += runtimes_tokens + 1; //+1 represent the runtimes JSMN_ARRAY

    //MSUs
    if (jsoneq(json_string, &t[i], "MSUs") != 0) {
        debug("DEBUG: Expected MSUs but was %.*s\n",
              t[i].end - t[i].start, json_string+t[i].start);
        return -1;
    }
    i++;
    int msu_cnt = t[i].size;

    debug("DEBUG: Detected %d MSUs in this dataflow graph.", msu_cnt);

    ret = json_parse_msu(t, i, msu_cnt, json_string);
    if (ret != 0) {
        debug("ERROR: %s", "could not extract msus from JSON");
        return -1;
    }

    fclose(fin);

    // Pick only msus where no scheduling item is initialized
    int to_allocate_msus[MAX_MSU];
    int n;
    for (n = 0; n < dfg.vertex_cnt; ++n) {
        if (dfg.vertices[n]->scheduling == NULL) {
            to_allocate_msus[n] = dfg.vertices[n]->msu_id;
        }
    }

    if (strcmp(load_mode, "toload") == 0) {
        //allocate(to_allocate_msus);
    }

    return 0;
}

/**
 * This section of the file defines DFG operations
 */

//Print the current dataflow graph
void print_dfg(void) {
    //FIXME: will not print all MSU/runtimes if, e.g, we create MSU with ID > vertex_cnt
    pthread_mutex_lock(dfg.dfg_mutex);

    printf("This DeDOS instance currently manages %d runtimes: \n", dfg.runtimes_cnt);

    if (dfg.runtimes_cnt > 0 ) {
        char ip[INET_ADDRSTRLEN];
        int r;

        for (r = 0; r < dfg.runtimes_cnt; r++) {
            if (dfg.runtimes[r] != NULL) {
                ipv4_to_string(&ip, dfg.runtimes[r]->ip);

                printf("runtime #%d. [ip] %s | [socket] %d | [cores] %d | [pin_t] %d | [threads] %d\n",
                        r + 1,
                        ip,
                        dfg.runtimes[r]->sock,
                        dfg.runtimes[r]->num_cores,
                        dfg.runtimes[r]->num_pinned_threads,
                        dfg.runtimes[r]->num_threads);
            }
        }
    }

    printf("The current dataflow graph has %d MSUs\n", dfg.vertex_cnt);
    if (dfg.vertex_cnt > 0 ) {
        int v = 0;
        for (v = 0; v < dfg.vertex_cnt; v++) {
            if (dfg.vertices[v] != NULL) {
                printf("The %d-th MSU: msu_id=%d, msu_type=%d, msu_mode=%s\n",
                        v + 1,
                        dfg.vertices[v]->msu_id,
                        dfg.vertices[v]->msu_type,
                        dfg.vertices[v]->msu_mode);
            }
        }
    }

    pthread_mutex_unlock(dfg.dfg_mutex);
}

/**
 * Functions to retrieve information about DFG elements
 */

/* Display information about runtimes */
int show_connected_peers(void) {
    char ipstr[INET_ADDRSTRLEN];
    int count = 0;
    int i;

    debug("INFO: %s", "Current open runtime peer sockets ->");

    pthread_mutex_lock(dfg.dfg_mutex);

    for (i = 0; i < dfg.runtimes_cnt; ++i) {
        if (dfg.runtimes[i]->sock != -1) {
            ipv4_to_string(ipstr, dfg.runtimes[i]->ip);
            debug("INFO => Socket: %d, IP: %s, Cores: %u, Pinned Threads: %u  Total Threads: %u",
                  dfg.runtimes[i]->sock,
                  ipstr,
                  dfg.runtimes[i]->num_cores,
                  dfg.runtimes[i]->num_pinned_threads,
                  dfg.runtimes[i]->num_threads);
            count++;
        }
    }

    pthread_mutex_unlock(dfg.dfg_mutex);

    return count;
}


/* Get runtimes IPs */
void get_connected_peer_ips(uint32_t *peer_ips) {
    int count = 0;
    int i;

    pthread_mutex_lock(dfg.dfg_mutex);

    for (i = 0; i < dfg.runtimes_cnt; ++i) {
        if (dfg.runtimes[i]->sock != -1) {
            peer_ips[count] = dfg.runtimes[i]->ip;
            count++;
        }
    }

    pthread_mutex_unlock(dfg.dfg_mutex);
}

/* Get runtime IP from socket */
uint32_t get_sock_endpoint_ip(int sock) {
    int i;

    pthread_mutex_lock(dfg.dfg_mutex);

    for (i = 0; i < dfg.runtimes_cnt; ++i) {
        if (dfg.runtimes[i]->sock == sock) {
            pthread_mutex_unlock(dfg.dfg_mutex);
            return dfg.runtimes[i]->ip;
        }
    }

    pthread_mutex_unlock(dfg.dfg_mutex);

    return -1;
}

/* Get runtime socket from IP */
int get_sock_endpoint_index(int sock) {
    int i;

    pthread_mutex_lock(dfg.dfg_mutex);

    for (i = 0; i < dfg.runtimes_cnt; ++i) {
        if (dfg.runtimes[i]->sock == sock) {
            pthread_mutex_unlock(dfg.dfg_mutex);
            return i;
        }
    }

    pthread_mutex_unlock(dfg.dfg_mutex);

    return -1;
}

/* Overwrite or create a MSU in the DFG */
void set_msu(struct dfg_vertex *msu) {
    debug("DEBUG: adding msu %d to the graph", msu->msu_id);

    pthread_mutex_lock(dfg.dfg_mutex);

    if (dfg.vertices[msu->msu_id - 1] != NULL) {
        free(dfg.vertices[msu->msu_id -1]);
    }

    dfg.vertices[msu->msu_id - 1] = msu;
    dfg.vertex_cnt++;

    pthread_mutex_unlock(dfg.dfg_mutex);
}

/* Big mess of create, updates, etc */
void update_dfg(struct dedos_dfg_manage_msg *update_msg) {
    debug("DEBUG: updating MSU for action: %d", update_msg->msg_code);

    pthread_mutex_lock(dfg.dfg_mutex);

    switch (update_msg->msg_code) {
        case MSU_ROUTE_ADD: {
            //TODO: don't assume that the src/dst msu did not exist in the table before.
            struct dedos_dfg_update_msu_route_msg *update;
            update = (struct dedos_dfg_update_msu_route_msg *) update_msg->payload;

            struct dfg_vertex *msu_from = dfg.vertices[update->msu_id_from - 1];
            struct dfg_vertex *msu_to = dfg.vertices[update->msu_id_to - 1];

            //TODO: handle "holes" in the list, due to MSU_ROUTE_DEL
            msu_from->msu_dst_list[msu_from->num_dst] = msu_to;
            msu_from->num_dst++;

            msu_to->msu_src_list[msu_to->num_src] = msu_from;
            msu_to->num_src++;

            free(update_msg);
            free(update);
            break;
        }

        case RUNTIME_ENDPOINT_ADD: {
            //because both in toload and preload mode we have already been adding some
            //runtimes in the DFG, we need to check if the new runtime isn't actually
            //an "expected" one, while allowing totally new runtimes to register.

            if (dfg.runtimes_cnt == MAX_RUNTIMES) {
                debug("ERROR: %s", "Number of runtimes limit reached.");
                break;
            }

            struct dedos_dfg_add_endpoint_msg *update;
            update = (struct dedos_dfg_add_endpoint_msg *) update_msg->payload;

            debug("DEBUG: registering endpoint at %s", update->runtime_ip);

            struct runtime_endpoint *r = NULL;

            int i;
            for (i = 0; i < dfg.runtimes_cnt; ++i) {
                char runtime_ip[INET_ADDRSTRLEN];
                ipv4_to_string(runtime_ip, dfg.runtimes[i]->ip);

                //We assume only one runtime per ip
                if (strcmp(runtime_ip, update->runtime_ip) == 0) {
                    r = dfg.runtimes[i];
                    break;
                }
            }

            if (r == NULL) {
                r = malloc(sizeof(struct runtime_endpoint));

                string_to_ipv4(update->runtime_ip, &(r->ip));

                dfg.runtimes[dfg.runtimes_cnt] = r;
                dfg.runtimes_cnt++;
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

    pthread_mutex_unlock(dfg.dfg_mutex);
}

/* Return a pointer to the dfg */
struct dfg_config *get_dfg() {
    return &dfg;
}
