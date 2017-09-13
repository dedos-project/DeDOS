#ifndef DFG_H_
#define DFG_H_

#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "unused_def.h"

// Putting these up here helps deal with circular dependency warnings
#define MAX_MSU 128
struct dfg_config;
struct dfg_vertex;

#define MAX_RUNTIMES 4
#define MAX_THREADS 32
#define MAX_ROUTES 32
#define MAX_ROUTE_DESTINATIONS 32
#define MAX_INIT_DATA_LEN 32
#define MAX_MSU_NAME_LEN 32
#define MAX_MSU_TYPES 32
#define MAX_APP_NAME_LENGTH 64

struct msu_init_data {
    char init_data[MAX_INIT_DATA_LEN];
};

struct dfg_runtime {
    int id;
    // removed: `int sock`, keeping in runtime or global controller
    uint32_t ip;
    int port;
    int n_cores;
    // removed: uint64_t dram;
    // removed: uint64_t io_network_bw;
    struct dfg_thread *threads[MAX_THREADS];
    int n_pinned_threads;
    int n_unpinned_threads;

    // removed: struct cut *current_alloc;
    struct dfg_route *routes[MAX_ROUTES];
    int n_routes;
};

enum thread_mode {
    UNSPECIFIED_THREAD_MODE = 0,
    PINNED_THREAD  = 1,
    UNPINNED_THREAD = 2
};

#define MAX_MSU_PER_THREAD 8

struct dfg_thread {
    int id;
    enum thread_mode mode;
    struct dfg_msu *msus[MAX_MSU_PER_THREAD];
    int n_msus;
    // removed: int utilization; //sum(msu.wcet / msu.deadline)
};

struct dfg_scheduling {
    struct dfg_runtime *runtime;
    struct dfg_thread *thread;
    struct dfg_route *routes[MAX_MSU_TYPES];
    int n_routes;
    // removed: float dedline;
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
    struct dfg_route_destination *destinations[MAX_ROUTE_DESTINATIONS];
    int n_destinations;
};


enum blocking_mode {
    BLOCKING_MSU = 1,
    NONBLOCK_MSU = 2
};

struct dfg_dependency;

struct dfg_msu_type {
    int id;
    char name[MAX_MSU_NAME_LEN];
    struct dfg_meta_routing meta_routing;

    struct dfg_dependency *dependencies[MAX_MSU_TYPES];
    int n_dependencies;

    struct dfg_msu *instances[MAX_MSU];
    int n_instances;

    int cloneable;
    int colocation_group;
};

enum msu_locality {
    MSU_IS_LOCAL = 1,
    MSU_IS_REMOTE = 2
};

struct dfg_dependency {
    struct dfg_msu_type *type;
    enum msu_locality locality;
};

// CHANGE: Replacing char *vertex_type with uint8 so we can 'or' ENTRY and EXIT
static uint8_t UNUSED ENTRY_VERTEX_TYPE = 0x01;
static uint8_t UNUSED EXIT_VERTEX_TYPE  = 0x02;

struct dfg_msu {
    int id;
    uint8_t vertex_type;
    struct msu_init_data init_data;

    struct dfg_msu_type *type;

    enum blocking_mode blocking_mode;

    // removed: struct dfg_profiling profiling;
    struct dfg_scheduling scheduling;
    // removed: removing msu_statistics_data in favor of storing in global controller
};


// Wrapper structure for the DFG
struct dedos_dfg {
    // removed: char init_dfg_filename[128];
    char application_name[64];
    // removed: float application_deadline;
    uint32_t global_ctl_ip;
    int global_ctl_port;

    struct dfg_msu_type *msu_types[MAX_MSU_TYPES];
    int n_msu_types;

    struct dfg_msu *msus[MAX_MSU];
    int n_msus;

    struct dfg_runtime *runtimes[MAX_RUNTIMES];
    int n_runtimes;
};

struct dfg_runtime *get_dfg_runtime(struct dedos_dfg *dfg, int id);
struct dfg_msu_type *get_dfg_msu_type(struct dedos_dfg *dfg, int id);
struct dfg_route *get_dfg_route(struct dedos_dfg *dfg, int id);
struct dfg_msu *get_dfg_msu(struct dedos_dfg *dfg, int id);
struct dfg_thread *get_dfg_thread(struct dfg_runtime *rt, int id);

#endif //DFG_H_
