#include "dfg.h"
#include "scheduling.h"
#include "controller_tools.h"
#include "logging.h"

jsmn_parser jp;
struct dfg_config dfg;    //The global config for DFG.

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
 * Parse JSON structure detailing scheduling data for an MSU
 * @param jsmntok_t *t a JSMN token
 * @param int starting_token where to start reading from the JSON string
 * @param const char *json_string the JSON string
 * @param struct dfg_vertex *v a node from the DFG
 * @return int success / no success
 */
void parse_msu_scheduling(jsmntok_t *t, int *starting_token,
                          const char *json_string, struct dfg_vertex *v) {
    int *i = starting_token;

    struct msu_scheduling *s = NULL;
    s = malloc(sizeof(struct msu_scheduling));
    if (s == NULL) {
        debug("%s", "Could not allocate memory for msu_scheduling");
    }

    v->scheduling = s;

    int f = 0;
    int fields = t[*i].size;
    //jump to first element in the object
    *i += 1;
    while (f < fields) {
        if (t[*i].type != JSMN_STRING) {
            debug("DEBUG: Expected JSMN_STRING but found %d\n", t[*i].type);
        }
        debug("String name : %.*s\n", t[*i].end-t[*i].start, json_string + t[*i].start);

        char value[256];
        memset(value, '\0', 256);
        memcpy(value, json_string + t[*i+1].start, t[*i+1].end - t[*i+1].start);

        if (jsoneq(json_string, &t[*i], "runtime_id") == 0) {
            debug("'runtime_id' field. Value: %s\n", value);

            v->scheduling->runtime = get_runtime_from_id(atoi(value));

            if (v->scheduling->runtime == NULL) {
                debug("Couldn't find runtime associated with id %d", atoi(value));
            }

            f++;
            *i += 2;
        } else if (jsoneq(json_string, &t[*i], "thread_id") == 0) {
            debug("'thread_id' field. Value: %s\n", value);

            v->scheduling->thread_id = atoi(value);

            f++;
            *i += 2;
        } else if (jsoneq(json_string, &t[*i], "deadline") == 0) {
            debug("'deadline' field. Value: %s\n", value);

            v->scheduling->deadline = atoi(value);

            f++;
            *i += 2;
        } else if (jsoneq(json_string, &t[*i], "routing") == 0) {
            debug("'routing' field. Value: %.*s\n\n",
                  t[*i+1].end - t[*i+1].start, json_string + t[*i+1].start);

            if (t[*i+1].type != JSMN_ARRAY) {
                debug("DEBUG: Expected JSMN_ARRAY but found %d\n", t[*i+1].type);
            }

            //jump to the ARRAY object
            *i += 1;
            int size = t[*i].size;

            if (size > 0) {
                struct dfg_edge_set *set = NULL;
                set = malloc(sizeof(struct dfg_edge_set));
                if (set == NULL) {
                    debug("%s", "Could not allocate memory for dfg_edge_set");
                }

                v->scheduling->routing = set;
                v->scheduling->routing->num_edges = size;
            } else {
                *i += 1;
                f++;
                continue;
            }

            char value[4];
            int dst;
            for (dst = 0; dst < size; ++dst) {
                //jump to first/next element in the array
                *i += 1;

                debug("destination id field. Value: %.*s\n\n",
                      t[*i].end - t[*i].start, json_string + t[*i].start);

                if (t[*i].type != JSMN_STRING) {
                    debug("DEBUG: Expected JSMN_STRING but found %d\n", t[*i].type);
                }

                struct dfg_edge *edge = NULL;
                edge = malloc(sizeof(struct dfg_edge));
                if (edge == NULL) {
                    debug("%s", "Could not allocate memory for dfg_edge");
                }
                v->scheduling->routing->edges[dst] = edge;

                memset(value, '\0', 4);
                memcpy(value, json_string + t[*i].start, t[*i].end - t[*i].start);

                v->scheduling->routing->edges[dst]->to = get_msu_from_id(atoi(value));
            }

            *i += 1;
            f++;
        } else {
            fprintf(stdout, "unknown key: %*.*s",
                    t[*i].end - t[*i].start, json_string + t[*i].start);
        }
    }
}

/**
 * Parse JSON structure detailing profiling data for an MSU
 * @param jsmntok_t *t a JSMN token
 * @param int starting_token where to start reading from the JSON string
 * @param const char *json_string the JSON string
 * @param struct dfg_vertex *v a node from the DFG
 * @return int success / no success
 */
int parse_msu_profiling(jsmntok_t *t, int starting_token,
                        const char *json_string, struct dfg_vertex *v) {
    int i = starting_token;

    if (t[i].type != JSMN_OBJECT) {
        debug("DEBUG: Expected %d-th field to be JSMN_OBJECT but found %d",
              i, t[i].type);
        return -1;
    }

    int fields = t[i].size;

    struct msu_profiling *p = NULL;
    p = malloc(sizeof(struct msu_profiling));
    if (p == NULL) {
        debug("%s", "Could not allocate memory for msu_profiling struct");
        return -1;
    }

    v->profiling = p;

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

            v->profiling->wcet = atoi(value);

        } else if (jsoneq(json_string, &t[i+s], "dram") == 0) {
            debug("'dram' field. Value: %s\n", value);

            v->profiling->dram = atol(value);

        } else if (jsoneq(json_string, &t[i+s], "tx_node_local") == 0) {
            debug("'tx_node_local' field. Value: %s\n", value);

            v->profiling->tx_node_local = atoi(value);

        } else if (jsoneq(json_string, &t[i+s], "tx_core_local") == 0) {
            debug("'tx_core_local' field. Value: %s\n", value);

            v->profiling->tx_core_local = atoi(value);

        } else if (jsoneq(json_string, &t[i+s], "tx_node_remote") == 0) {
            debug("'tx_node_remote' field. Value: %s\n", value);

            v->profiling->tx_node_remote = atoi(value);

        } else {
            fprintf(stdout, "unknown key: %*.*s",
                    t[i+s].end - t[i+s].start, json_string + t[i+s].start);
            return -1;
        }
    }

    return 0;
}

/**
 * Parse JSON structure detailing runtimes
 * @param jsmntok_t *t a JSMN token
 * @param int starting_token where to start reading from the JSON string
 * @param int ending_token where to sttop reading from the JSON string
 * @param const char *json_string the JSON string
 * @return int success / no success
 */
int json_parse_runtimes(jsmntok_t *t, int starting_token,
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

/**
 * Parse JSON structure detailing MSUs
 * @param jsmntok_t *t a JSMN token
 * @param int starting_token where to start reading from the JSON string
 * @param int msu_cnt the number of MSU to parse
 * @param const char *json_string the JSON string
 * @return int success / no success
 */
int json_parse_msu(jsmntok_t *t, int starting_token, int msu_cnt, const char *json_string) {
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
        int f = 0;

        //Create a new MSU:
        struct dfg_vertex *v = NULL;
        v = (struct dfg_vertex*)calloc(1, sizeof(struct dfg_vertex));
        dfg.vertices[dfg.vertex_cnt] = v;
        dfg.vertex_cnt++;

        //init some base fields
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
            } else if (jsoneq(json_string, &t[i], "vertex_type") == 0) {
                debug("'vertex_type' field. Value: %.*s\n\n",
                      t[i+1].end - t[i+1].start, json_string + t[i+1].start);

                if (t[i+1].type != JSMN_STRING) {
                    debug("DEBUG: Expected JSMN_STRING but found %d\n", t[i+1].type);
                    return -1;
                }

                memcpy(v->vertex_type, json_string + t[i+1].start, t[i+1].end - t[i+1].start);

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
                debug("'meta_routing' field. Value: %.*s\n\n",
                      t[i+1].end - t[i+1].start, json_string + t[i+1].start);

                if (t[i+1].type != JSMN_OBJECT) {
                    debug("DEBUG: Expected JSMN_OBJECT but found %d\n", t[i+1].type);
                    return -1;
                }

                struct msu_meta_routing *m = NULL;
                m = malloc(sizeof(struct msu_meta_routing));
                if (m == NULL) {
                    debug("%s", "Could not allocate memory for msu_meta_routing struct");
                    return -1;
                }
                v->meta_routing = m;

                //jump to first item in the meta_routing object
                i += 2;

                int num_src_types, num_dst_types;

                if (jsoneq(json_string, &t[i], "source_types") == 0 &&
                    strncmp(v->vertex_type, "entry", strlen("entry\0")) != 0) {
                    debug("'source_types' field. Value: %.*s\n\n",
                          t[i+1].end - t[i+1].start, json_string + t[i+1].start);

                    if (t[i+1].type != JSMN_ARRAY) {
                        debug("DEBUG: Expected JSMN_ARRAY but found %d\n", t[i+1].type);
                        return -1;
                    }

                    v->meta_routing->num_src_types = 0;
                    num_src_types = t[i+1].size;

                    //jump to the first element of the array
                    i += 2;

                    int s;
                    char value[4];
                    for (s = 0; s < num_src_types; s++) {
                        debug("New source MSU: %.*s\n\n",
                          t[i].end - t[i].start, json_string + t[i].start);

                        if (t[i].type != JSMN_STRING) {
                            debug("DEBUG: Expected JSMN_STRING but found %d\n", t[i].type);
                            return -1;
                        }

                        memset(value, '\0', 4);
                        memcpy(value, json_string + t[i].start, t[i].end - t[i].start);
                        v->meta_routing->src_types[v->meta_routing->num_src_types] = atoi(value);
                        v->meta_routing->num_src_types++;

                        //jump to next source (last jump will be to dst_types)
                        i++;
                    }
                }

                if (jsoneq(json_string, &t[i], "dst_types") == 0 &&
                    strncmp(v->vertex_type, "exit", strlen("exit\0")) != 0) {
                    debug("'dst_types' field. Value: %.*s\n\n",
                          t[i].end - t[i].start, json_string + t[i].start);

                    if (t[i+1].type != JSMN_ARRAY) {
                        debug("DEBUG: Expected JSMN_ARRAY but found %d\n", t[i+1].type);
                        return -1;
                    }

                    v->meta_routing->num_dst_types = 0;
                    num_dst_types = t[i+1].size;

                    //jump to the first element of the array
                    i += 2;

                    int d;
                    char value[4];
                    for (d = 0; d < num_dst_types; d++) {
                        debug("New dst MSU: %.*s\n\n",
                          t[i].end - t[i].start, json_string + t[i].start);

                        if (t[i].type != JSMN_STRING) {
                            debug("DEBUG: Expected JSMN_STRING but found %d\n", t[i].type);
                            return -1;
                        }

                        memset(value, '\0', 4);
                        memcpy(value, json_string + t[i].start, t[i].end - t[i].start);
                        v->meta_routing->dst_types[v->meta_routing->num_dst_types] = atoi(value);
                        v->meta_routing->num_dst_types++;

                        //jump to next destination
                        i++;
                    }
                }

                f++;
            } else if (jsoneq(json_string, &t[i], "scheduling") == 0) {
                debug("'scheduling' field. Value: %.*s\n\n",
                      t[i+1].end - t[i+1].start, json_string + t[i+1].start);

                if (t[i+1].type != JSMN_OBJECT) {
                    debug("DEBUG: Expected JSMN_OBJECT but found %d\n", t[i+1].type);
                    return -1;
                }

                //jump in the object
                i++;
                parse_msu_scheduling(t, &i, json_string, v);

                f++;
            } else {
                fprintf(stdout, "unknown key: %*.*s",
                        t[i].end - t[i].start, json_string + t[i].start);
                return -1;
            }
        }

        c++;
    }

    return 0;
}

/**
 * Parse JSON file and initialize the controller's internal state.
 * Two modes of operation: "preload" and "toload". If an MSU has a "scheduling" object,
 * we consider it as already deployed. This function will call allocate() to compute an
 * allocation scheme for all MSUs no having a scheduling object
 * @param char *init_cfg_filename JSON filename
 * @return int success / no success
 */
int do_dfg_config(const char * init_cfg_filename) {
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
    jsmntok_t t[4096];//num. of JSON tokens.
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

    if (jsoneq(json_string, &t[i], "application_deadline") != 0) {
        debug("DEBUG: Expected 'application_deadline' but was %.*s\n",
              t[i].end - t[i].start, json_string+t[i].start);
        return -1;
    }
    char deadline[5];
    strncpy(deadline, json_string + t[i+1].start, t[i+1].end - t[i+1].start);
    dfg.application_deadline = atoi(deadline);
    debug("application_deadline %d", dfg.application_deadline);
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
    struct to_schedule *ts = NULL;
    ts = malloc(sizeof(struct to_schedule));
    int to_allocate_msus[MAX_MSU];
    int num_to_alloc = 0;

    int n;
    for (n = 0; n < dfg.vertex_cnt; ++n) {
        if (dfg.vertices[n]->scheduling == NULL) {
            to_allocate_msus[n] = dfg.vertices[n]->msu_id;
            num_to_alloc++;
        }
    }

    ts->msu_ids = to_allocate_msus;
    ts->num_msu = num_to_alloc;

    print_dfg();
    dump_json();

    if (strcmp(load_mode, "toload") == 0) {
        //allocate(ts);
    }

    return 0;
}

/**
 * Parse the DFG and format it as a JSON string
 * @return none 
 */
void dump_json(void) {
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
            field_len = 9 + how_many_digits(dfg.vertices[m]->profiling->wcet) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"wcet\":\"%d\"",
                                 dfg.vertices[m]->profiling->wcet);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //footprint
            field_len = 18 + how_many_digits(dfg.vertices[m]->profiling->dram) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"mem_footprint\":\"%d\"",
                                 dfg.vertices[m]->profiling->dram);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //tx_node_local
            field_len = 18 + how_many_digits(dfg.vertices[m]->profiling->tx_node_local) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"tx_node_local\":\"%d\"",
                                 dfg.vertices[m]->profiling->tx_node_local);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //tx_core_local
            field_len = 18 + how_many_digits(dfg.vertices[m]->profiling->tx_core_local) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"tx_core_local\":\"%d\"",
                                 dfg.vertices[m]->profiling->tx_core_local);
            json_len += snprintf(json_string + json_len, 3, ", ");

            //tx_node_remote
            field_len = 19 + how_many_digits(dfg.vertices[m]->profiling->tx_node_remote) + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"tx_node_remote\":\"%d\"",
                                 dfg.vertices[m]->profiling->tx_node_remote);

            json_len += snprintf(json_string + json_len, 3, "},");

            //meta routing
            field_len = 17 + 1;
            json_len += snprintf(json_string + json_len,
                                 field_len,
                                 "\"meta_routing\":{");

            //sources
            int has_sources = 0;
            if (strncmp(dfg.vertices[m]->vertex_type, "entry", strlen("entry\0")) != 0 &&
                dfg.vertices[m]->meta_routing->num_src_types > 0) {

                has_sources = 1;

                field_len = 17 + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"sources_types\":[");

                int s;
                for (s = 0; s < dfg.vertices[m]->meta_routing->num_src_types; ++s) {
                    if (s > 0) {
                        json_len += snprintf(json_string + json_len, 3, ", ");
                    }

                    //id
                    field_len = 2 + how_many_digits(dfg.vertices[m]->meta_routing->src_types[s]) + 1;
                    json_len += snprintf(json_string + json_len,
                                         field_len,
                                         "\"%d\"",
                                         dfg.vertices[m]->meta_routing->src_types[s]);

                }

                json_len += snprintf(json_string + json_len, 2, "]");
            }

            //destinations
            if (strncmp(dfg.vertices[m]->vertex_type, "exit", strlen("entry\0")) != 0 &&
                dfg.vertices[m]->meta_routing->num_dst_types > 0) {

                if (has_sources == 1) {
                    json_len += snprintf(json_string + json_len, 3, ", ");
                }

                field_len = 13 + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"dst_types\":[");

                int s;
                for (s = 0; s < dfg.vertices[m]->meta_routing->num_dst_types; ++s) {
                    if (s > 0) {
                        json_len += snprintf(json_string + json_len, 3, ", ");
                    }

                    //id
                    field_len = 2 + how_many_digits(dfg.vertices[m]->meta_routing->dst_types[s]) + 1;
                    json_len += snprintf(json_string + json_len,
                                         field_len,
                                         "\"%d\"",
                                         dfg.vertices[m]->meta_routing->dst_types[s]);

                }

                json_len += snprintf(json_string + json_len, 2, "]");
            }

            json_len += snprintf(json_string + json_len, 2, "}");

            //scheduling
            if (dfg.vertices[m]->scheduling != NULL) {
                json_len += snprintf(json_string + json_len, 3, ", ");
                field_len = 14 + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"scheduling\":{");

                field_len = 15 + how_many_digits(dfg.vertices[m]->scheduling->runtime->id) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"runtime_id\":\"%d\"",
                                     dfg.vertices[m]->scheduling->runtime->id);
                json_len += snprintf(json_string + json_len, 3, ", ");

                field_len = 14 + how_many_digits(dfg.vertices[m]->scheduling->thread_id) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"thread_id\":\"%d\"",
                                     dfg.vertices[m]->scheduling->thread_id);
                json_len += snprintf(json_string + json_len, 3, ", ");

                field_len = 13 + how_many_digits(dfg.vertices[m]->scheduling->deadline) + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"deadline\":\"%d\"",
                                     dfg.vertices[m]->scheduling->deadline);


                if (dfg.vertices[m]->scheduling->routing != NULL) {
                    if (dfg.vertices[m]->scheduling->routing->num_edges > 0) {
                        json_len += snprintf(json_string + json_len, 3, ", ");

                        field_len = 17 + 1;
                        json_len += snprintf(json_string + json_len,
                                             field_len,
                                             "\"destinations\": [");
                        int d;
                        for (d = 0; d < dfg.vertices[m]->scheduling->routing->num_edges; ++d) {
                            if (d > 0) {
                                json_len += snprintf(json_string + json_len, 3, ", ");
                            }

                            int msu_id =
                                    dfg.vertices[m]->scheduling->routing->edges[d]->to->msu_id;

                            field_len = 2 + how_many_digits(msu_id) + 1;
                            json_len += snprintf(json_string + json_len,
                                                 field_len,
                                                 "\"%d\"",
                                                 msu_id);
                        }

                        json_len += snprintf(json_string + json_len, 2, "]");
                    }

                }

                json_len += snprintf(json_string + json_len, 2, "}");
            }

            //statistics
            if (dfg.vertices[m]->statistics != NULL) {
                json_len += snprintf(json_string + json_len, 3, ", ");
                field_len = 14 + 1;
                json_len += snprintf(json_string + json_len,
                                     field_len,
                                     "\"statistics\":{");

                json_len += snprintf(json_string + json_len, 2, "}");
            }


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
                printf("The %d-th MSU: msu_id=%d, msu_type =%d, msu_mode=%s\n",
                        v + 1,
                        dfg.vertices[v]->msu_id,
                        dfg.vertices[v]->msu_type,
                        dfg.vertices[v]->msu_mode);
            }
        }
    }
}

/**
 * Return a pointer to the dfg
 * @return struct dfg_config &dfg
 */
struct dfg_config *get_dfg() {
    return &dfg;
}
