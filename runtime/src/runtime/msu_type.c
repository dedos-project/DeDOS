/**
 * @file msu_type.c
 *
 * Registration and location of msu types
 */

#include "dfg.h"
#include "msu_type.h"
#include "logging.h"

#define MAX_TYPE_ID 1000

static const struct msu_type *MSU_TYPES[] = {
    /*
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
    */
};

/**
 * Pointers to MSU Types, indexed by ID
 */
static struct msu_type *msu_types[MAX_TYPE_ID];

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
                  type->name, type->id, MAX_TYPE_ID);
        return -1;
    }
    msu_types[type->id] = type;
    log_custom(LOG_MSU_TYPE_INIT, "Registered MSU type %s", type->name);
    return 0;
}

/**
 * Gets the MSU type with the provided ID
 * @param id The type ID of the MSU type to retrieve
 * @return The MSU Type with the given ID, NULL in N/A
 */
struct msu_type *get_msu_type(int id) {
    if (id > MAX_MSU_TYPES) {
        log_error("MSU type %d cannot be found Type ID too high. Max: %d",
                  id, MAX_TYPE_ID);
        return NULL;
    }
    return msu_types[id];
}

static bool has_required_fields(struct msu_type *type) {
    if (type->receive == NULL) {
        log_error("MSU type %s does not register a recieve function",
                  type->name);
        return false;
    }
    return true;
}

static int init_msu_type(struct msu_type *type) {
    if (!has_required_fields(type)) {
        log_error("Not registering MSU type %s due to missing fields", 
                  type->name);
        return -1;
    }
    if (type->init_type) {
        if (type->init_type(type) != 0) {
            log_error("MSU Type %s not initialized: failed custom constructor",
                      type->name);
            return -1;
        } else {
            log_custom(LOG_MSU_TYPE_INIT, "Initialized msu type %s", type->name);
        }
    }
    if (register_msu_type(type) != 0) {
        log_error("MSU type %s not registered", type->name);
        return -1;
    }
    return 0;
}

/**
 * Initializes the MSU type with the given ID, 
 * calling the custom constructor if appropriate. 
 * @return 0 on success, -1 on error
 */
int init_msu_type_id(unsigned int type_id) {
    for (int i=0; i<N_MSU_TYPES; i++) {
        struct msu_type *type = msu_types[i];
        if (type->id == type_id) {
            return init_msu_type(type);
        }
    }
    log_error("MSU Type ID %u not found!", type_id);
    return -1;
}
