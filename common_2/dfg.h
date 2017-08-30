#ifndef DFG_H_
#define DFG_H_

#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "defines.h"

// Putting these up here helps deal with circular dependency warnings
#define MAX_MSU 128
struct dfg_config;
struct dfg_vertex;

#define MAX_RUNTIMES 4
#define MAX_THREADS 32
#define MAX_ROUTES 32
#define MAX_INIT_DATA_LEN 32
#define MAX_MSU_NAME_LEN 32
#define MAX_MSU_TYPES 32

struct dfg_runtime {
    int id;
    // REMOVED: `int sock`, keeping in runtime or global controller
    uint32_t ip;
    int port;
    int n_cores;
    // REMOVED: uint64_t dram;
    // REMOVED: uint64_t io_network_bw;
    // REMOVED: uint32_t num_pinned threads (can search thread structure to determine)
    struct dfg_thread *threads[MAX_THREADS];
    int n_threads;
    // REMOVED: struct cut *current_alloc;
    struct dfg_route *routes[MAX_ROUTES];
    int n_routes;
};

enum thread_mode {
    PINNED_THREAD  = 1,
    UNPINNED_THREAD = 2
};

#define MAX_MSU_PER_THREAD 8

struct dfg_thread {
    int id;
    enum thread_mode mode;
    struct dfg_msu *msus[MAX_MSU_PER_THREAD];
    int n_msus;
    // IMP: int utilization; //sum(msu.wcet / msu.deadline)
};

struct dfg_scheduling {
    struct dfg_runtime *runtime;
    struct dfg_thread *thread;
    struct dfg_route *routes[MAX_MSU_TYPES];
    int n_routes;
    int cloneable;
    int colocation_group;
    // IMP: float dedline;
};

struct dfg_meta_routing {
    struct dfg_msu_type *src_types[MAX_MSU];
    int n_src_types;
    struct dfg_msu_type *dst_types[MAX_MSU];
    int n_dst_types;
};

struct dfg_route_destination {
    int key;
    struct dfg_msu *msu;
};

struct dfg_route {
    int id;
    struct dfg_msu_type *msu_type;
    struct dfg_route_destinations *destinations;
    int n_destinations;
};

// IMP: Replacing char *vertex_type
static uint8_t UNUSED ENTRY_VERTEX_TYPE = 0x01;
static uint8_t UNUSED EXIT_VERTEX_TYPE  = 0x02;


enum blocking_mode {
    BLOCKING_MSU,
    NONBLOCK_MSU
};

struct dfg_dependency;

struct dfg_msu_type {
    int type_id;
    uint8_t vertex_type;
    char name[MAX_MSU_NAME_LEN];
    struct dfg_meta_routing meta_routing;
    int n_dependencies;
    struct dfg_dependency *dependencies[MAX_MSU_TYPES];
};

enum msu_locality {
    MSU_IS_LOCAL,
    MSU_IS_REMOTE
};

struct dfg_dependency {
    struct dfg_msu_type type;
    enum msu_locality locality;
};

struct dfg_msu {
    uint8_t vertex_type;
    char init_data[MAX_INIT_DATA_LEN];

    int id;
    struct dfg_msu_type *type;

    char name[MAX_MSU_NAME_LEN];
    enum blocking_mode blocking_mode;

    // TODO: Profiling in DFG?
    //struct dfg_profiling profiling;
    struct dfg_scheduling scheduling;
    // IMP: removing msu_statistics_data in favor of storing in global controller
};

// Wrapper structure for the DFG
struct dedos_dfg {
    // IMP: char init_dfg_filename[128];
    // IMP: char application_name[64];
    // IMP: float application_deadline;
    uint32_t controller_ip; // IMP: global_ctl_ip
    int controller_port; // IMP: global_ctl_port

    struct dfg_msu *msus[MAX_MSU]; //IMP: vertices
    int n_msu; //IMP vertex_cnt;

    struct dfg_runtime *runtimes[MAX_RUNTIMES];
    int n_runtimes; //IMP: runtimes_cnt

    // IMP: dfg_mutex
};

struct dfg_runtime *get_dfg_runtime(struct dedos_dfg *dfg, int runtime_id);

#endif //DFG_H_
