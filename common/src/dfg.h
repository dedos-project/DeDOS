#ifndef DFG_H_
#define DFG_H_

#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>

// Putting these up here helps deal with circular dependency warnings
#define MAX_MSU 128
struct dfg_config;
struct dfg_vertex;

/* Some infra properties */
#define MAX_THREADS 20
#define MAX_RUNTIMES 20
#define NUM_MSU_TYPES 16
#define MAX_DESTINATIONS 16

#include "timeseries.h"
#include "logging.h"
#include "scheduling_cut.h"

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
struct dfg_runtime_endpoint {
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
    int num_routes;
    struct dfg_route *routes[MAX_MSU];
};

struct runtime_thread {
    int id;
    int mode; // not-pinned/pinned
    //TODO: define a more reasonable upper limit on msu per thread
    struct dfg_vertex *msus[MAX_MSU];
    int num_msus;
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
    struct timed_rrdb queue_items_processed;
    struct timed_rrdb queue_length;
    struct timed_rrdb memory_allocated;
    struct timed_rrdb execution_time;
    struct timed_rrdb n_states;
};

struct msu_scheduling {
    struct dfg_runtime_endpoint *runtime;
    int thread_id;
    // TODO: ^ and v are redundant
    struct runtime_thread *thread;
    int num_routes;
    struct dfg_route *routes[NUM_MSU_TYPES];
    float deadline;
    int cloneable;
    int colocation_group;
};

//store the absolute destination and source types
struct msu_meta_routing {
    int src_types[MAX_MSU];
    int num_src_types;
    int dst_types[MAX_MSU];
    int num_dst_types;
};

struct dfg_route {
    int route_id;
    int msu_type;
    int num_destinations;
    int destination_keys[MAX_DESTINATIONS];
    struct dfg_vertex *destinations[MAX_DESTINATIONS];
};

struct dependent_type {
    int msu_type;
    int locality; // 1/0 -- local/remote
};

//Definition of a vertex--an MSU
struct dfg_vertex {
    char vertex_type[12]; //entry, exit, ...
    char init_data[32];

    //Meta data as parsed from the dataflow graph file.
    int msu_id;
    int msu_type;
    char msu_name[128]; //Name of the MSU: "handshaking"
    char msu_mode[13]; //blocking/non-blocking

    int num_dependencies;

    struct dependent_type dependencies[NUM_MSU_TYPES];
    //Profiling data
    struct msu_profiling profiling;

    //Scheduling data
    struct msu_scheduling scheduling;

    //Routing data
    struct msu_meta_routing meta_routing;

    //Monitoring data
    struct msu_statistics_data statistics;
};

// Wrapper structure for the DFG
struct dfg_config {
    char init_dfg_filename[128];      //Initial dataflow graph file.

    char global_ctl_ip[INET_ADDRSTRLEN];
    int global_ctl_port;
    char application_name[64];

    //scheduling data
    float application_deadline;

    //Data-flow graph of MSUs
    struct dfg_vertex *vertices[MAX_MSU]; //All MSUs
    int vertex_cnt;                       //Number of MSUs
    struct dfg_runtime_endpoint *runtimes[MAX_RUNTIMES]; //All runtimes
    int runtimes_cnt;
    pthread_mutex_t dfg_mutex;
};

extern struct dfg_config dfg_config_g;

//Some utility struct
struct msus_of_type {
    int *msu_ids;
    int num_msus;
    int type;
};

/*Runtime & DFG management functions */
void update_dfg(struct dedos_dfg_manage_msg *update_msg);
struct dfg_config *get_dfg();
int get_connected_peer_ips(uint32_t *peer_ips);
uint32_t get_sock_endpoint_ip(int sock);
int get_sock_endpoint_index(int sock);
int show_connected_peers(void);
struct dfg_runtime_endpoint *get_runtime_from_id(int runtime_id);
int lookup_type_on_runtime(struct dfg_runtime_endpoint *rt, int msu_type);

/* MSU management functions */
void set_msu(struct dfg_vertex *msu);
struct msus_of_type *get_msus_from_type(int type);
struct dfg_vertex *get_msu_from_id(int msu_id);
struct dependent_type *get_dependent_type(struct dfg_vertex *msu, int msu_type);
void clone_type_static_data(struct dfg_vertex *msu);

/* Route management functions */
int increment_max_range(struct dfg_route *route);
int generate_msu_id();
int generate_route_id(struct dfg_runtime_endpoint *rt);
int dfg_add_route(struct dfg_runtime_endpoint *rt, int route_id, int msu_type);
int add_route_to_msu_vertex(int runtime_index, int msu_id, int route_id);
int del_route_from_msu_vertex(int runtime_index, int msu_id, int route_id) ;
int dfg_mod_route_endpoint(int runtime_index, int route_id, int msu_id, unsigned int range_end);
int dfg_add_route_endpoint(int runtime_index, int route_id, int msu_id, unsigned int range_end);
int dfg_del_route_endpoint(int runtime_index, int route_id, int msu_id);
struct dfg_route *get_msu_route_from_type(struct dfg_vertex *msu, int msu_type);
struct dfg_route *get_route_from_type(struct dfg_runtime_endpoint *rt, int msu_type);
struct dfg_route *get_route_from_id(struct dfg_runtime_endpoint *rt, int route_id);
int msu_has_route(struct dfg_vertex *msu, int route_id);
int route_has_endpoint(struct dfg_route *route, struct dfg_vertex *msu);

#endif //DFG_H_
