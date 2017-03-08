#ifndef DFG_H
#define DFG_H

#include "logging.h"
#include "communication.h"
#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>

// Counts are inclusive of ALL subfields
#define RUNTIME_FIELDS 6
#define MSU_PROFILING_FIELDS 10
#define MAX_JSON_LEN 65536
#define MAX_MSU 128

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

struct msu_profiling_data {
    int wcet;
    long dram_footprint;
    int tx_node_local;
    int tx_core_local;
    int tx_node_remote;
};

struct msu_statistics_data {
    int msu_events_this_intvl;
    int msu_events_last_intvl;
};

struct msu_scheduling {
    struct runtime_endpoint *msu_runtime;
    int weight;
    int thread_id;
};

//store the absolute destionation and soure types
struct msu_meta_routing {
    int src_types[16];
    int num_src;
    int dst_types[16];
    int num_dst;
};

//Definition of a vertex--an MSU
struct dfg_vertex {
    char vertex_name[128];            //Name of the MSU: "handshaking"

    struct dfg_vertex *next[24];     //Next hops of the MSUs

    //Meta data as parsed from the dataflow graph file.
    int msu_id;
    int msu_type;
    int thread_id;
    struct runtime_endpoint *msu_runtime;
    char msu_mode[12]; //blocking/non-blocking

    //Profiling data
    struct msu_profiling_data *profiling_data;

    //Scheduling data
    struct msu_scheduling *scheduling;

    //Routing data
    struct msu_meta_routing *meta_routing;
    struct dfg_vertex *msu_src_list[MAX_MSU]; //might change in the future
    int num_src;
    struct dfg_vertex *msu_dst_list[MAX_MSU];
    int num_dst;
    char msu_src_ip[INET_ADDRSTRLEN];
    int msu_src_port;
    char msu_dst_ip[INET_ADDRSTRLEN];
    int msu_dst_port;
};

struct dfg_config {
    char init_dfg_filename[128];      //Initial dataflow graph file.

    char global_ctl_ip[INET_ADDRSTRLEN];
    int global_ctl_port;
    char application_name[64];

    //Data-flow graph of MSUs
    struct dfg_vertex *entry;             //The entry point MSU
    struct dfg_vertex *vertices[MAX_MSU]; //All MSUs
    int vertex_cnt;                       //Number of MSUs
    struct runtime_endpoint *runtimes[MAX_RUNTIMES];
    int runtimes_cnt;
    pthread_mutex_t *dfg_mutex;
};

extern struct dfg_config dfg_config_g;

int do_dfg_config(char *init_filename);
int load_init_dfg();
void print_dfg();
void set_msu(struct dfg_vertex *msu);
void update_dfg(struct dedos_dfg_manage_msg *update_msg);
struct dfg_config *get_dfg();
void get_connected_peer_ips(uint32_t *peer_ips);
uint32_t get_sock_endpoint_ip(int sock);
int get_sock_endpoint_index(int sock);
int show_connected_peers(void);

#endif //Dataflow graph
