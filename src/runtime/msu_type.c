/**
 * @file msu_type.c
 *
 * Registration and location of msu types
 */

#include "dfg.h"
#include "msu_type.h"
#include "logging.h"
#include "msu_type_list.h"

/**
 * MOVEME: MAX_TYPE_ID
 * This is the maximum ID that can be assigned to an MSU type
 */
#define MAX_TYPE_ID 1000

/**
 * Every MSU type that can be used. Defined in `msu_type_list.h`,
 * or overridden from previous definition
 */
static struct msu_type *DEFINED_MSU_TYPES[] = MSU_TYPE_LIST;

/**
 * The number of available MSU types
 */
#define N_MSU_TYPES (sizeof(DEFINED_MSU_TYPES) / sizeof(struct msu_type*))

/**
 * Pointers to MSU Types, indexed by ID
 */
static struct msu_type *msu_types[MAX_TYPE_ID];

/**
 * Regsiters an MSU type so that it can be later referenced by its ID
 * @param type MSU Type to be registered
 * @return 0 on success, -1 on error
 */
static int register_msu_type(struct msu_type *type) {
    if (type->id > MAX_TYPE_ID) {
        log_error("MSU type %s not registered. Type ID %d too high. Max: %d",
                  type->name, type->id, MAX_TYPE_ID);
        return -1;
    }
    msu_types[type->id] = type;
    log(LOG_MSU_TYPE_INIT, "Registered MSU type %s", type->name);
    return 0;
}

void destroy_msu_types() {
    for (int i=0; i < N_MSU_TYPES; i++) {
        struct msu_type *type = DEFINED_MSU_TYPES[i];
        if (msu_types[type->id] != NULL) {
            if (msu_types[type->id]->destroy_type != NULL) {
                msu_types[type->id]->destroy_type(type);
            }
        }
    }
}

struct msu_type *get_msu_type(int id) {
    if (id > MAX_TYPE_ID) {
        log_error("MSU type %d cannot be found Type ID too high. Max: %d",
                  id, MAX_TYPE_ID);
        return NULL;
    }
    return msu_types[id];
}

/**
 * Checks whether the msu type has the required fields to be used.
 * (At the moment, the only required field is `receive`)
 * @return true/false
 */
static bool has_required_fields(struct msu_type *type) {
    if (type->receive == NULL) {
        log_error("MSU type %s does not register a recieve function",
                  type->name);
        return false;
    }
    return true;
}

/**
 * Initializes a specific MSU type, calling its init function
 * and registering it for future calls
 * @param type The type of MSU to be registered
 * @return 0 on success, -1 on error
 */
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
            log(LOG_MSU_TYPE_INIT, "Initialized msu type %s", type->name);
        }
    }
    if (register_msu_type(type) != 0) {
        log_error("MSU type %s not registered", type->name);
        return -1;
    }
    return 0;
}


int init_msu_type_id(unsigned int type_id) {
    log(TEST, "Number of MSU types: %d", (int)N_MSU_TYPES);
    for (int i=0; i<N_MSU_TYPES; i++) {
        struct msu_type *type = DEFINED_MSU_TYPES[i];
        if (type->id == type_id) {
            return init_msu_type(type);
        }
    }
    log_error("MSU Type ID %u not found!", type_id);
    return -1;
}
