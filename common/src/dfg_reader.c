#include <strings.h>
#include <string.h>
#include "ip_utils.h"
#include "dfg.h"
#include "dfg_reader.h"
#include "jsmn_parser.h"
#include "jsmn.h"

enum object_type {
    ROOT=0, RUNTIMES=1, ROUTES=2, DESTINATIONS=3, MSUS=4, PROFILING=5,
    META_ROUTING=6, SOURCE_TYPES=7, SCHEDULING=8
};

struct dfg_config *dfg_global = NULL;

struct dfg_config *get_dfg(){
    return dfg_global;
}

int load_dfg_from_file(const char *init_filename){
    dfg_global = parse_dfg_json(init_filename);
    if (dfg_global == NULL){
        log_error("Error loading dfg from file %s!", init_filename);
        return -1;
    }
    return 0;
}

static struct key_mapping key_map[];

struct dfg_config *parse_dfg_json(const char *filename){

    struct dfg_config *cfg = malloc(sizeof(*cfg));
    bzero(cfg, sizeof(*cfg));

    int rtn = parse_into_obj(filename, cfg, key_map);

    if (rtn >= 0){
        pthread_mutex_init(&cfg->dfg_mutex, NULL);
        log_debug("Success!");
        return cfg;
    } else {
        log_error("Failed to parse jsmn");
        return NULL;
    }
}

static int ignore(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    return 0;
}

static int set_app_name(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_config *cfg = in->data;
    strcpy(cfg->application_name, tok_to_str(*tok, j));
    return 0;
}

static int set_app_deadline(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_config *cfg = in->data;
    cfg->application_deadline = tok_to_int(*tok, j);
    return 0;
}

static int set_ctl_ip(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_config *cfg = in->data;
    strcpy(cfg->global_ctl_ip, tok_to_str(*tok, j));
    return 0;
}

static int set_ctl_port(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_config *cfg = in->data;
    cfg->global_ctl_port = tok_to_int(*tok, j);
    return 0;
}

static int set_load_mode(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    char *load_mode = tok_to_str(*tok, j);
    if (strcasecmp(load_mode, "preload")){
        return 0;
    } else { // TODO: What if preload/toload?
        return 0;
    }
}

struct json_state init_runtime(struct json_state *in, int index){
    struct dfg_config *cfg = in->data;

    cfg->runtimes_cnt++;
    cfg->runtimes[index] = malloc(sizeof(*cfg->runtimes[index]));

    struct json_state rt_obj = {
        .data = cfg->runtimes[index],
        .parent_type = RUNTIMES
    };

    return rt_obj;
}

static int set_runtimes(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    return parse_jsmn_obj_list(tok, j, in, saved, init_runtime);
}

static struct json_state init_dfg_route(struct json_state *in, int index){
    struct dfg_runtime_endpoint *rt = in->data;

    rt->num_routes++;
    rt->routes[index] = malloc(sizeof(*rt->routes[index]));

    struct json_state route_obj = {
        .data = rt->routes[index],
        .parent_type = ROUTES
    };

    return route_obj;
}

static int set_routes(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    return parse_jsmn_obj_list(tok, j, in, saved, init_dfg_route);
}

struct json_state init_dfg_msu(struct json_state *in, int index){
    struct dfg_config *cfg = in->data;

    cfg->vertex_cnt++;
    cfg->vertices[index] = malloc(sizeof(*cfg->vertices[index]));

    struct json_state msu_obj = {
        .data = cfg->vertices[index],
        .parent_type = MSUS
    };
    return msu_obj;
}

static int set_msus(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    return parse_jsmn_obj_list(tok, j, in, saved, init_dfg_msu);
}

static int set_msu_vertex_type(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_vertex *vertex = in->data;
    strcpy(vertex->vertex_type, tok_to_str(*tok, j));
    return 0;
}

static int set_msu_profiling(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_vertex *vertex = in->data;
    struct json_state profiling = {
        .data = &vertex->profiling,
        .parent_type = PROFILING,
        .tok = *tok
    };
    int n_parsed = parse_jsmn_obj(tok, j, &profiling, saved);
    if (n_parsed < 0){
        return -1;
    }
    return 0;
}

static int set_meta_routing(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_vertex *vertex = in->data;
    struct json_state meta_routing = {
        .data = &vertex->meta_routing,
        .parent_type = META_ROUTING,
        .tok = (*tok)
    };
    int n_parsed = parse_jsmn_obj(tok, j, &meta_routing, saved);
    if (n_parsed < 0){
        return -1;
    }
    return 0;
}

static int set_working_mode(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_vertex *vertex = in->data;
    strcpy(vertex->msu_mode, tok_to_str(*tok, j));
    return 0;
}

static int set_scheduling(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_vertex *vertex = in->data;
    struct json_state scheduling = {
        .data = &vertex->scheduling,
        .parent_type = SCHEDULING,
        .tok = *tok
    };
    int n_parsed = parse_jsmn_obj(tok, j, &scheduling, saved);
    if (n_parsed < 0){
        return -1;
    }
    return 0;
}

static int set_msu_type(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_vertex *vertex = in->data;
    vertex->msu_type = tok_to_int(*tok, j);
    return 0;
}

static int set_msu_id(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_vertex *vertex = in->data;
    vertex->msu_id = tok_to_int(*tok, j);
    log_debug("Set msu ID %d", vertex->msu_id);
    return 0;
}

static int set_rt_dram(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_runtime_endpoint *runtime = in->data;
    runtime->dram = tok_to_int(*tok, j);
    return 0;
}

static int set_rt_ip(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_runtime_endpoint *runtime = in->data;
    char *ipstr = tok_to_str(*tok, j);
    string_to_ipv4(ipstr, &runtime->ip);
    return 0;
}

static int set_rt_bw(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_runtime_endpoint *runtime = in->data;
    runtime->io_network_bw = tok_to_int(*tok, j);
    return 0;
}

static int set_rt_n_cores(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_runtime_endpoint *runtime = in->data;
    runtime->num_cores = tok_to_int(*tok, j);
    return 0;
}

static int set_rt_id(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_runtime_endpoint *runtime = in->data;
    runtime->id = tok_to_int(*tok, j);
    return 0;
}

static int set_rt_port(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_runtime_endpoint *runtime = in->data;
    runtime->port = tok_to_int(*tok, j);
    return 0;
}

static int set_route_id(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_route *route = in->data;
    route->route_id = tok_to_int(*tok, j);
    return 0;
}

static int set_route_type(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_route *route = in->data;
    route->msu_type = tok_to_int(*tok, j);
    return 0;
}

static int set_route_destination(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_route *route = in->data;
    struct dfg_config *cfg =  get_jsmn_obj();
    int n_dests = (*tok)->size;
    if (cfg->vertex_cnt == 0){
        (*tok) += (n_dests * 2);
        return 1;
    }
    ASSERT_JSMN_TYPE(*tok, JSMN_OBJECT, j);
    for (int i=0; i<n_dests; i++){
        ++(*tok);
        ASSERT_JSMN_TYPE(*tok, JSMN_STRING, j);
        log_debug("looking for msu %s", tok_to_str(*tok, j));
        int msu_id = tok_to_int(*tok, j);
        int found = 0;
        for (int msu_i=0; msu_i<cfg->vertex_cnt; msu_i++){
            if (cfg->vertices[msu_i]->msu_id == msu_id){
                found = 1;
                ++(*tok);
                ASSERT_JSMN_TYPE(*tok, JSMN_STRING, j);
                int key = tok_to_int(*tok, j);
                struct dfg_vertex *v = cfg->vertices[msu_i];
                route->destinations[route->num_destinations] = v;
                route->destination_keys[route->num_destinations] = key;
                ++route->num_destinations;
                break;
            }
        }
        if (!found){
            log_error("Could not find msu %d for route %d", msu_id, route->route_id);
            return -1;
        }
    }
    return 0;
}

static int set_prof_dram(jsmntok_t  **tok, char *j, struct json_state *in, struct json_state **saved){
    struct msu_profiling *profiling = in->data;
    profiling->dram = tok_to_long(*tok, j);
    return 0;
}

static int set_tx_node_local(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct msu_profiling *profiling = in->data;
    profiling->tx_node_local = tok_to_int(*tok, j);
    return 0;
}

static int set_wcet(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct msu_profiling *profiling = in->data;
    profiling->wcet = tok_to_int(*tok, j);
    return 0;
}

static int set_tx_node_remote(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct msu_profiling *profiling = in->data;
    profiling->tx_node_remote = tok_to_int(*tok, j);
    return 0;
}

static int set_tx_core_local(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct msu_profiling *profiling = in->data;
    profiling->tx_core_local = tok_to_int(*tok, j);
    return 0;
}

static int set_source_types(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct msu_meta_routing *meta = in->data;
    int n_srcs = (*tok)->size;
    for (int i=0; i<n_srcs; i++){
        ++(*tok);
        int type = tok_to_int(*tok, j);
        meta->src_types[meta->num_src_types] = type;
        ++meta->num_src_types;
    }
    return 0;
}

static int set_dst_types(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct msu_meta_routing *meta = in->data;
    int n_dests = (*tok)->size;
    for (int i=0; i<n_dests; i++){
        ++(*tok);
        int type = tok_to_int(*tok, j);
        meta->dst_types[meta->num_dst_types] = type;
        ++meta->num_dst_types;
    }
    return 0;
}

static int set_thread_id(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct msu_scheduling *sched = in->data;
    sched->thread_id = tok_to_int(*tok, j);
    return 0;
}

static int set_deadline(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct msu_scheduling *sched = in->data;
    sched->deadline = tok_to_int(*tok, j);
    return 0;
}

static int set_msu_rt_id(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    struct dfg_config *cfg = get_jsmn_obj();
    if (cfg->runtimes_cnt == 0){
        return 1;
    }

    struct msu_scheduling *sched = in->data;
    ASSERT_JSMN_TYPE(*tok, JSMN_STRING, j);
    int id = tok_to_int(*tok, j);

    for (int i=0; i<cfg->runtimes_cnt; i++){
        struct dfg_runtime_endpoint *rt = cfg->runtimes[i];
        if (rt->id == id){
            sched->runtime = rt;
            return 0;
        }
    }
    log_error("Could not find runtime with id %d (%d present runtimes)", id, cfg->runtimes_cnt);
    return -1;
}

static int set_msu_routing(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    int n_routes = (*tok)->size;
    struct dfg_config *cfg = get_jsmn_obj();
    struct msu_scheduling *sched = in->data;
    if (cfg->runtimes_cnt == 0 ||
            sched->runtime == NULL){
        (*tok) += n_routes;
        return 1;
    }
    ASSERT_JSMN_TYPE(*tok, JSMN_ARRAY, j);
    for (int i=0; i<n_routes; i++){
        ++(*tok);
        int route_id = tok_to_int(*tok, j);
        int found = 0;
        for (int route_i=0; route_i < sched->runtime->num_routes; route_i ++){
            if (sched->runtime->routes[route_i]->route_id == route_id){
                found = 1;
                struct dfg_route *route = sched->runtime->routes[route_i];
                sched->routes[sched->num_routes] = route;
                ++sched->num_routes;
                break;
            }
        }
        if (!found){
            log_error("Could not find required route %d", route_id);
            return -1;
        }
    }
    return 0;
}


static struct key_mapping key_map[] = {
    { "application_name", ROOT, set_app_name },
    { "application_deadline", ROOT, set_app_deadline },
    { "global_ctl_ip", ROOT, set_ctl_ip },
    { "global_ctl_port", ROOT, set_ctl_port },
    { "load_mode", ROOT, set_load_mode },

    { "runtimes", ROOT, set_runtimes },
    { "MSUs", ROOT, set_msus },

    { "vertex_type", MSUS, set_msu_vertex_type },
    { "profiling", MSUS,  set_msu_profiling },
    { "meta_routing", MSUS,  set_meta_routing },
    { "working_mode", MSUS,  set_working_mode },
    { "scheduling", MSUS,  set_scheduling },
    { "type", MSUS,  set_msu_type },
    { "id", MSUS,  set_msu_id },
    { "name", MSUS, ignore },

    { "dram", RUNTIMES, set_rt_dram },
    { "ip", RUNTIMES,  set_rt_ip },
    { "io_network_bw", RUNTIMES, set_rt_bw },
    { "num_cores", RUNTIMES, set_rt_n_cores },
    { "id", RUNTIMES, set_rt_id },
    { "port", RUNTIMES, set_rt_port },
    { "routes", RUNTIMES, set_routes },

    { "id", ROUTES, set_route_id },
    { "destinations", ROUTES, set_route_destination },
    { "type", ROUTES, set_route_type },

    { "dram", PROFILING, set_prof_dram },
    { "tx_node_local", PROFILING,  set_tx_node_local },
    { "wcet", PROFILING,  set_wcet },
    { "tx_node_remote", PROFILING,  set_tx_node_remote },
    { "tx_core_local", PROFILING,  set_tx_core_local },

    { "source_types", META_ROUTING,  set_source_types },
    { "dst_types", META_ROUTING, set_dst_types },

    { "thread_id", SCHEDULING,  set_thread_id },
    { "deadline", SCHEDULING,  set_deadline },
    { "runtime_id", SCHEDULING,  set_msu_rt_id },
    { "routing", SCHEDULING, set_msu_routing },

    { NULL, 0, NULL }
};

