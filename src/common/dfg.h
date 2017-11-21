/**
 * @file dfg.h
 *
 * Interfaces for the creation and modification of the data-flow-graph and
 * and general description of the application running within DeDOS.
 */

#ifndef DFG_H_
#define DFG_H_

#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>

// Putting these up here helps deal with circular dependency warnings
struct dfg_config;
struct dfg_vertex;

/** The maximum number of MSUs which can be present in the system at a time */
#define MAX_MSU 512

/** The ID to which messages should be addressed when delivered to the main runtime thread */
#define MAIN_THREAD_ID 0

/** The maximum number of runtimes that may be present in the DFG */
#define MAX_RUNTIMES 16
/** The maximum number of threads that may be present on a runtime */
#define MAX_THREADS 32
/** The total maximum number of routes that may be present in the DFG */
#define MAX_ROUTES 64
/** The maximum number of endpoints that a single route may have as destinations */
#define MAX_ROUTE_ENDPOINTS 256
/** The maximum length of the initial data that may be passed to an MSU */
#define MAX_INIT_DATA_LEN 64
/** The maximum length of the name for an MSU type */
#define MAX_MSU_NAME_LEN 32
/** The maximum number of different MSU types that may be present in the DFG */
#define MAX_MSU_TYPES 32
/** The maximum length for the name of the application */
#define MAX_APP_NAME_LENGTH 64
/** The maximum number of MSUs which can be placed on a single thread */
#define MAX_MSU_PER_THREAD 8

/**
 * Data with which an MSU is initialized, and the payload for messages of type ::CTRL_CREATE_MSU
 */
struct msu_init_data {
    char init_data[MAX_INIT_DATA_LEN];
};

/**
 * Representation of a runtime in the DFG
 */
struct dfg_runtime {
    int id;      /**< Unique identifier for the runtime */
    uint32_t ip; /**< IP of the node on which the runtime is running */
    int port;    /**< Port on which the runtime is listening for controller/inter-runtime */
    int n_cores; /**< Number of cores on the runtime node */

    struct dfg_thread *threads[MAX_THREADS]; /**< Threads located on the runtime */
    int n_pinned_threads;   /**< Number of the above-threads which are pinned */
    int n_unpinned_threads; /**< Number of the above-threads which are unpinned */

    struct dfg_route *routes[MAX_ROUTES]; /**< Routes located on this runtime */
    int n_routes; /**< Number of routes above */
    // todo: uint64_t dram;
    // todo: struct cut *current_alloc;
    // todo: uint64_t io_network_bw;
};

/** Identifies if a thread is pinned to a core or able to be scheduled on any core */
enum thread_mode {
    UNSPECIFIED_THREAD_MODE = 0, /**< Default -- threads should not have this mode */
    PINNED_THREAD  = 1,
    UNPINNED_THREAD = 2
};

/**
 * Converts a string of pinned/unpinned to the corresponding enumerator.
 * String should be "pinned" or "unpinned" (case insensitive).
 * */
enum thread_mode str_to_thread_mode(char *mode);

/** Representation of a thread on a runtime in the DFG */
struct dfg_thread {
    int id;  /**< Unique identifier for the thread */
    enum thread_mode mode; /**< Pinned/unpinned mode for the thread */
    struct dfg_msu *msus[MAX_MSU_PER_THREAD]; /**< MSUs placed on that thread */
    int n_msus; /**< Number of MSUs placed on the thread */

    // todo: int utilization; //sum(msu.wcet / msu.deadline)
};

/**
 * Structure representing the scheduling of an MSU on a runtime.
 */
struct dfg_scheduling {
    struct dfg_runtime *runtime; /**< The runtime on which an MSU is running */
    struct dfg_thread *thread;   /**< The thread on which an MSU is running */
    struct dfg_route *routes[MAX_MSU_TYPES]; /** The routes that an MSU can send to */
    int n_routes; /** The number of routes the msu can send to */
    // ???: Should `routes` really be located in this structure?

    // todo: float dedline;
};

/**
 * Describes which MSU types a given MSU type should route to if it is cloned
 */
struct dfg_meta_routing {
    struct dfg_msu_type *src_types[MAX_MSU]; /**< The types that should route to this MSU */
    int n_src_types; /**< The number of types that should route to this MSU */
    struct dfg_msu_type *dst_types[MAX_MSU]; /**< The types that this msu should route to */
    int n_dst_types; /**< The number of types that this msu should route to */
};

/**
 * A single endpoint for an MSU route
 */
struct dfg_route_endpoint {
    uint32_t key;        /**< The key associated with this endpoint.
                              In default routing schemes, determines the proportion of traffic
                              sent to this MSU */
    struct dfg_msu *msu; /**< The MSU at this endpoint to which a message would be delivered*/
};

/**
 * A route through which MSU messages can be passed.
 * A route consists of a series of MSUs to which a message can be delivered.
 * Multiple MSUs can send through the same route, as long as it exists on the MSU's runtime.
 * A route can only send to MSUs of a single type.
 */
struct dfg_route {
    int id; /**< A unique identifier for the route */
    struct dfg_runtime *runtime;   /**< The runtime on which the route is located */
    struct dfg_msu_type *msu_type; /**< The type of MSU to which this route delivers */
    struct dfg_route_endpoint *endpoints[MAX_ROUTE_ENDPOINTS]; /**< The endpoints of the route */
    int n_endpoints; /**< The number of endpoints in dfg_route::endpoints */
};

/** Whether an MSU is blocking or non-blocking */
enum blocking_mode {
    UNKNOWN_BLOCKING_MODE = 0, /**< Default value -- used when unset, should never be specified */
    BLOCKING_MSU = 1,
    NONBLOCK_MSU = 2
};
/**
 * Converts a string of blocking/non-blocking to the correct enumerator.
 * String should be "blocking" or "non-blocking" (or "nonblocking") (case insensitive).
 */
enum blocking_mode str_to_blocking_mode(char *mode_str);

// Forward declaration; defined below
struct dfg_dependency;

/** A type of MSU. Shared across all instances of that MSU type */
struct dfg_msu_type {
    int id; /**< A unique identifier for the MSU type.
              Should match that defined in the MSU's header */
    char name[MAX_MSU_NAME_LEN]; /**< A name describing the function of the MSU */
    struct dfg_meta_routing meta_routing; /**< Which types of msus route to/from this MSU */

    /** These MSU types must be present in order for this MSU type to be cloned */
    struct dfg_dependency *dependencies[MAX_MSU_TYPES];
    int n_dependencies; /**< The number of elements in dfg_msu_type::dependencies */

    struct dfg_msu *instances[MAX_MSU]; /**< Each instance of this MSU type */
    int n_instances; /**< The number of instances of this MSU type */

    int cloneable;  /**< If cloneable == N, this MSU can be cloned on runtimes numbered
                         up to and including N */
    int colocation_group; /** MSU types which have the same colocation group (if > 0)
                              can be scheduled on the same thread */
};

/** Whether an MSU is located on the same runtime or a remote runtime */
enum msu_locality {
    UNDEFINED_LOCALITY = 0, /**< Default value -- not to be used */
    MSU_IS_LOCAL = 1,
    MSU_IS_REMOTE = 2
};

/** MSUs which must be present for another MSU to be cloned */
struct dfg_dependency {
    struct dfg_msu_type *type;  /**< The MSU type which must be present */
    enum msu_locality locality; /**< Whether it must be present on the same machine */
};

/** Bitmask representing an MSU through which messages enter DeDOS */
#define ENTRY_VERTEX_TYPE 0x01
/** Bitmask representing an MSU through which messages exit DeDOS */
#define EXIT_VERTEX_TYPE  0x02
/** Converts a string containing exit and/or entry to the correct bitmask */
uint8_t str_to_vertex_type(char *type_str);

/** Representation of a single MSU in the dfg */
struct dfg_msu {
    int id; /**< A unique identifier for the MSU */
    uint8_t vertex_type; /**< Whether the MSU is #ENTRY, #EXIT, or possible #ENTRY | #EXIT */
    struct msu_init_data init_data; /**< Initial data passed to the MSU */

    struct dfg_msu_type *type; /**< The type of the MSU and meta-routing information */

    enum blocking_mode blocking_mode; /**< Whether the MSU is blocking or not */

    struct dfg_scheduling scheduling; /**< Information about where an MSU is scheduled*/
    // todo: struct dfg_profiling profiling;
};

/** Info to connect and use database */
struct db_info {
    uint32_t ip;
    int port;
    char name[16];
    char user[16];
    char pwd[16];
};

/** Top-level structure holding the data-flow graph */
struct dedos_dfg {
    char application_name[64]; /**< Description of the whole application */
    uint32_t global_ctl_ip;    /**< IP address of the global controller */
    int global_ctl_port;       /**< Port of the global controller */

    /** DB information */
    struct db_info db;

    /** MSU types which may be present in the application */
    struct dfg_msu_type *msu_types[MAX_MSU_TYPES];
    /** The number of elements in dedos_dfg::msu_types */
    int n_msu_types;

    /** The MSUs present in the application */
    struct dfg_msu *msus[MAX_MSU];
    /** The number of MSUs in dedos_dfg::msus */
    int n_msus;

    /** The runtimes present in the application */
    struct dfg_runtime *runtimes[MAX_RUNTIMES];
    /** The number of elements in dedos_dfg::runtimes */
    int n_runtimes;

    // todo: float application_deadline;
};

/**
 * Sets the local copy of the DFG, so it doesn't have to be passed in for each call.
 * @param dfg The copy of the DFG to set statically
 */
void set_dfg(struct dedos_dfg *dfg);

/** Returns DB info */
struct db_info *get_db_info();
/** Returns the number of registered runtime */
int get_dfg_n_runtimes();
/** Returns the runtime with the given ID */
struct dfg_runtime *get_dfg_runtime(unsigned int id);
/** Returns the MSU type with the given ID */
struct dfg_msu_type *get_dfg_msu_type(unsigned int id);
/** Returns the route with the given ID */
struct dfg_route *get_dfg_route(unsigned int id);
/** Returns the route on the given runtime with the specified MSU type */
struct dfg_route *get_dfg_rt_route_by_type(struct dfg_runtime *rt, struct dfg_msu_type *route_type);
/** Returns the route which the given MSU sends to of the specified MSU type */
struct dfg_route *get_dfg_msu_route_by_type(struct dfg_msu *msu, struct dfg_msu_type *route_type);
/** Returns the MSU with the given ID */
struct dfg_msu *get_dfg_msu(unsigned int id);
/** Returns the thread on the given runtime with the specified ID */
struct dfg_thread *get_dfg_thread(struct dfg_runtime *rt, unsigned int id);
/** Returns the endpoint within the given route which has the specified MSU ID */
struct dfg_route_endpoint *get_dfg_route_endpoint(struct dfg_route *route, unsigned int msu_id);

/** Returns 1 if the given MSU has the route as an endpoint */
int msu_has_route(struct dfg_msu *msu, struct dfg_route *route);
/** Returns 1 if the given MSU type is present on the provided runtime */
struct dfg_msu *msu_type_on_runtime(struct dfg_runtime *rt, struct dfg_msu_type *type);

/** Allocates a new MSU with the same fields as the input MSU (though unscheduled) */
struct dfg_msu *copy_dfg_msu(struct dfg_msu *input);
/** Allocates a new MSU with the given parameters */
struct dfg_msu *init_dfg_msu(unsigned int id, struct dfg_msu_type *type, uint8_t vertex_type,
                             enum blocking_mode mode, struct msu_init_data *init_data);
/** Frees an MSU structure */
int free_dfg_msu(struct dfg_msu *input);

/** Places the given MSU on the correct runtime and thread */
int schedule_dfg_msu(struct dfg_msu *msu, unsigned int runtime_id, unsigned int thread_id);
/** Removes the given MSU from its runtime and thread */
int unschedule_dfg_msu(struct dfg_msu *msu);

/** Creates a route with the specified parameters */
struct dfg_route *create_dfg_route(unsigned int id, struct dfg_msu_type *type,
                                   unsigned int runtime_id);
/** Deletes the provided route from the DFG */
int delete_dfg_route(struct dfg_route *route);

/** Subscribes an MSU to a route, so it can send to the route's endpoints */
int add_dfg_route_to_msu(struct dfg_route *route, struct dfg_msu *msu);
// TODO: del_dfg_route_from_msu()

/** Adds an MSU as an endpoint to a route */
struct dfg_route_endpoint *add_dfg_route_endpoint(struct dfg_msu *msu, uint32_t key,
                                                  struct dfg_route *route);
/** Removes an MSU as a route endpoint */
int del_dfg_route_endpoint(struct dfg_route *route, struct dfg_route_endpoint *ep);
/** Modifies the key associated with an MSU endpoint */
int mod_dfg_route_endpoint(struct dfg_route *route, struct dfg_route_endpoint *ep,
                           uint32_t new_key);

/** Creates a new thread on the given runtime */
struct dfg_thread * create_dfg_thread(struct dfg_runtime *rt, int thread_id,
                                      enum thread_mode mode);

/** Frees the entirety of the DFG structure */
void free_dfg(struct dedos_dfg *dfg);

#endif //DFG_H_
