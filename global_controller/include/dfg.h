#ifndef DFG_H_
#define DFG_H_

#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "statistics.h"
#include "logging.h"
#include "communication.h"
#include "jsmn.h"

/* Some infra properties */
#define MAX_RUNTIMES 20
#define MAX_THREADS 99
#define MAX_RCV_BUFLEN 4096

/* Some JSON properties */
#define MAX_JSON_LEN 131072
#define MAX_MSU 128
// Counts are inclusive of ALL subfields
#define RUNTIME_FIELDS 6
#define MSU_PROFILING_FIELDS 10

/**
 * JSON related declarations
 */
int do_dfg_config(const char *init_filename);
void print_dfg();
void dump_json(void);
struct dfg_config *get_dfg();
int jsoneq(const char *json, jsmntok_t *tok, const char *s);
void json_parse_error(int err);
int json_test(jsmntok_t *t, int r);
struct dfg_vertex; //for next signatures to refer to this
void parse_msu_scheduling(jsmntok_t *t, int *starting_token,
                          const char *json_string, struct dfg_vertex *v);
int parse_msu_profiling(jsmntok_t *t, int starting_token,
                        const char *json_string, struct dfg_vertex *v);
int json_parse_runtimes(jsmntok_t *t, int starting_token,
                        int ending_token, const char *json_string);
int json_parse_msu(jsmntok_t *t, int starting_token, int msu_cnt, const char *json_string);


/**
 * DFG related declarations
 */

/* Some struct related to DFG management protocol */
struct dedos_dfg_add_endpoint_msg {
    int runtime_sock;
    char runtime_ip[INET_ADDRSTRLEN];
};

struct dedos_dfg_update_msu_route_msg {
    int msu_id_from;
    int msu_id_to;
};

struct dedos_dfg_manage_msg {
    int msg_type;
    int msg_code;
    int header_len;
    int payload_len;
    void *payload;
};


/* Structures defining DFG elements */
struct runtime_endpoint {
    int id;
    int sock;
    uint32_t ip;
    uint16_t port;
    uint16_t num_cores;
    uint64_t dram; // 0 to 1.844674407×10^19
    uint64_t io_network_bw; // 0 to 1.844674407×10^19
    //TODO: move threads related fields in cut structure
    uint32_t num_pinned_threads; //for now we assume non-blocking workers =  pinned threads
    uint32_t num_threads;
    struct runtime_thread *threads[MAX_THREADS];
    struct cut *current_alloc;
};

struct runtime_thread {
    int id;
    int mode; // not-pinned/pinned
    //TODO: define a more reasonable upper limit on msu per thread
    struct dfg_vertex *msus[MAX_MSU];
    int utilization; //sum(msu.wcet / msu.deadline)
};

struct msu_profiling {
    int wcet;
    uint64_t dram;
    int tx_node_local;
    int tx_core_local;
    int tx_node_remote;
};

struct msu_statistics_data {
    struct timeserie *queue_item_processed;
    struct timeserie *data_queue_size;
    struct timeserie *memory_allocated;
};

struct msu_scheduling {
    struct runtime_endpoint *runtime;
    uint32_t thread_id;
    struct dfg_edge_set *routing;
    int deadline;
};

//store the absolute destination and source types
struct msu_meta_routing {
    int src_types[MAX_MSU];
    int num_src_types;
    int dst_types[MAX_MSU];
    int num_dst_types;
};

struct dfg_edge {
    struct dfg_vertex *from;
    struct dfg_vertex *to;
};

struct dfg_edge_set {
    int num_edges;
    struct dfg_edge *edges[MAX_MSU];
};

//Definition of a vertex--an MSU
struct dfg_vertex {
    char vertex_type[12]; //entry, exit, ...

    //Meta data as parsed from the dataflow graph file.
    int msu_id;
    int msu_type;
    char msu_name[128]; //Name of the MSU: "handshaking"
    char msu_mode[13]; //blocking/non-blocking

    //Profiling data
    struct msu_profiling *profiling;

    //Scheduling data
    struct msu_scheduling *scheduling;

    //Routing data
    struct msu_meta_routing *meta_routing;

    //Monitoring data
    struct msu_statistics_data *statistics;
};

// Wrapper structure for the DFG
struct dfg_config {
    char init_dfg_filename[128];      //Initial dataflow graph file.

    char global_ctl_ip[INET_ADDRSTRLEN];
    int global_ctl_port;
    char application_name[64];

    //scheduling data
    int application_deadline;

    //Data-flow graph of MSUs
    struct dfg_vertex *vertices[MAX_MSU]; //All MSUs
    int vertex_cnt;                       //Number of MSUs
    struct runtime_endpoint *runtimes[MAX_RUNTIMES]; //All runtimes
    int runtimes_cnt;

    pthread_mutex_t *dfg_mutex;
};

extern struct dfg_config dfg_config_g;


//Some utility struct
struct msus_of_type {
    int *msu_ids;
    int num_msus;
    int type;
};

void set_msu(struct dfg_vertex *msu);
void update_dfg(struct dedos_dfg_manage_msg *update_msg);
struct dfg_config *get_dfg();
void get_connected_peer_ips(uint32_t *peer_ips);
uint32_t get_sock_endpoint_ip(int sock);
int get_sock_endpoint_index(int sock);
int show_connected_peers(void);
struct dfg_vertex *get_msu_from_id(int msu_id);
struct msus_of_type *get_msus_from_type(int type);
struct runtime_endpoint *get_runtime_from_id(int runtime_id);

#endif //DFG_H_
