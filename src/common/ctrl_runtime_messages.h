#ifndef CTRL_RUNTIME_MESSAGES_H_
#define CTRL_RUNTIME_MESSAGES_H_
#include "unistd.h"
#include "dfg.h"
#include <stdbool.h>

enum ctrl_runtime_msg_type {
    // Main thread messages
    CTRL_CONNECT_TO_RUNTIME,
    CTRL_CREATE_THREAD,
    CTRL_DELETE_THREAD,
    CTRL_MODIFY_ROUTE,
    // TODO: CTRL_REPORT,

    // Worker thread messages
    CTRL_CREATE_MSU,
    CTRL_DELETE_MSU,
    CTRL_MSU_ROUTES
};

/**
 * Messages to runtime from ctrl are of this type
 */
struct ctrl_runtime_msg_hdr {
    enum ctrl_runtime_msg_type type;
    int id;              /**<< Id for confirmation message (not implemented) */
    int thread_id;       /**<< Thread to deliver message to */
    size_t payload_size; /**<< Payload will be serialized following this struct */
};


///////////////////////
// Main thread messages
///////////////////////

/** Type: CTRL_ADD_RUNTIME */
struct ctrl_add_runtime_msg {
    unsigned int runtime_id;
    uint32_t ip;
    int port;
};

/** Type: CTRL_CREATE_THREAD */
struct ctrl_create_thread_msg {
    int thread_id;
    enum blocking_mode mode;
};

/** Type: CTRL_DELETE_THREAD */
struct ctrl_delete_thread_msg {
    int thread_id;
};

/** Type: CTRL_MODIFY_ROUTE */
enum ctrl_route_msg_type {
    CREATE_ROUTE,
    DELETE_ROUTE,
    ADD_ENDPOINT,
    DEL_ENDPOINT,
    MOD_ENDPOINT
};

/** For creating routes or adding endpoints _to_ routes */
struct ctrl_route_msg {
    enum ctrl_route_msg_type type;
    int route_id;  
    int type_id;    /*<< Only needed if creating a new route */
    int msu_id;     /*<< Only needed if dealing with an endpoint*/
    int key;        /*<< Only filled if adding or modifying an endpoint */
    
    unsigned int runtime_id; /*<< Only needed if adding an endpoint */
};

///////////////////////
// Worker thread messages
///////////////////////

#define MAX_INIT_DATA_LEN 32

/** Type: CTRL_CREATE_MSU */
struct ctrl_create_msu_msg {
    // Note: Thread ID is specified in runtime_msg, not here
    int msu_id;
    int type_id;
    struct msu_init_data init_data;
    // TODO: Replace init_data with payload of variable size
};

struct ctrl_delete_msu_msg {
    bool force;
    int msu_id;
};

enum ctrl_msu_route_type {
    ADD_ROUTE,
    DEL_ROUTE
};

/** Type: CTRL_MSU_ROUTES, For add/rm routes _to_ an MSU */
struct ctrl_msu_route_msg {
    enum ctrl_msu_route_type type;
    int route_id;
    int msu_id;
};

#endif
