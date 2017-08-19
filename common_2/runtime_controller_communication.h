#ifndef ROUTING_CONTROL_H
#define ROUTING_CONTROL_H

//TODO: Rename to controller_runtime_communication.c

enum route_mod_type {
    CREATE_ROUTE,
    // TODO: DELETE_ROUTE,
    ADD_ENDPOINT,
    DEL_ENDPOINT,
    MOD_ENDPOINT
};

struct route_modification_msg {
    enum route_mod_type type;
    int route_id;
    int msu_id; /*<< Only filled if adding or changing an endpoint */
    int key;    /*<< Only filled if adding or changing an endpoint */
};

struct msu_route_msg {
    int route_id;
    int msu_id;
};

#define MAX_INIT_DATA_LEN 32

struct msu_init_data {
    char init_data[MAX_INIT_DATA_LEN];
};

struct delete_msu_msg {
    int msu_id;
};

struct create_msu_msg {
    int msu_id;
    int msu_type_id;
    struct msu_init_data init_data;
};

enum control_msg_type {
    RUNTIME_MSG,
    THREAD_MSG,
    MSU_MSG
};

struct runtime_control_msg {
    enum control_msg_type type;
    size_t data_size;
}

#endif
