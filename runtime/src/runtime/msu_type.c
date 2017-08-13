/**
 * @file msu_type.c
 *
 * Registration and location of msu types
 */

#include "msu_type.h"

#define MAX_MSU_ID 1000
#define MAX_MSU_TYPES 32

static struct msu_types[MAX_MSU_TYPES];
static int n_msu_types = 0;
static unsigned int msu_indices[MAX_MSU_ID];

static const struct msu_type *MSU_TYPES[] = {
    &PICO_TCP_MSU_TYPE,
    &HS_REQUEST_ROUTING_MSU_TYPE,
    &MSU_APP_TCP_ECHO_TYPE,
    &TCP_HANDSHAKE_MSU_TYPE,

    &BAREMETAL_MSU_TYPE,

    &BLOCKING_SOCKET_HANDLER_MSU_TYPE,
    &SOCKET_REGISTRY_MSU_TYPE,

    &WEBSERVER_HTTP_MSU_TYPE,
    &WEBSERVER_READ_MSU_TYPE,
    &WEBSERVER_REGEX_MSU_TYPE,
    &WEBSERVER_WRITE_MSU_TYPE,
    &WEBSERVER_REGEX_ROUTING_MSU_TYPE
};

#define N_MSU_TYPES (sizeof(MSU_TYPES) / sizeof(struct msu_type*))
/**
 * Regsiters an MSU type so that it can be later
 * referenced by its ID
 * @param type MSU Type to be registered
 * @return 0 on success, -1 on error
 */
static int register_msu_type(struct msu_type *type) {
    if (type->id > MAX_MSU_TYPES) {
        log_error("MSU type %s not registered. Type ID %d too high. Max: %d", 
                  type->name, type->id, MAX_MSU_ID);
        return -1;
    }
    msu_indices[type->id] = n_msu_types;
    msu_types[n_msu_types] = type;
    n_msu_types++;
    return 0;
}

/**
 * Initializes all MSU types, registering and optionally calling init function
 */
int init_msu_types() {
    for (int i=0; i<N_MSU_TYPES; i++) {
        struct msu_type *type = msu_types[i];
        if (type->init_type != NULL) {
            if (type->init_type(type) != 0) {
                log_warn("MSU Type %s not initialized", type->name);
                continue;
            }
        }
        if (register_msu_type(type) != 0) {
            log_warn("MSU Type %s not registered", type->name);
        }
    }
    return 0;
}
