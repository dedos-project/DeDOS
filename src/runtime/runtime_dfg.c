/**
 * @file: runtime_dfg.c
 * Interactions with the global dfg from an individual runtime's perspective
 */
#include "runtime_dfg.h"
#include "dfg_reader.h"
#include "logging.h"
#include "dfg_instantiation.h"
#include "msu_type.h"

#include <stdlib.h>

/** Static (global) variable for accessing a lodaed Dfg */
static struct dedos_dfg *DFG = NULL;
/** Static (global) variable holding this runtime's dfg */
static struct dfg_runtime *LOCAL_RUNTIME = NULL;

void set_local_runtime(struct dfg_runtime *rt) {
    LOCAL_RUNTIME = rt;
}

int init_runtime_dfg(char *filename, int runtime_id) {

    if (DFG != NULL) {
        log_error("Runtime DFG already instantiated!");
        return -1;
    }

    DFG = parse_dfg_json_file(filename);
    if (DFG == NULL) {
        log_error("Error initializing DFG");
        return -1;
    }
    set_dfg(DFG);

    LOCAL_RUNTIME = get_dfg_runtime(runtime_id);
    if (LOCAL_RUNTIME == NULL) {
        log_error("Error finding runtime %d in DFG %s",
                  runtime_id, filename);
        return -1;
    }

    if (init_dfg_msu_types(DFG->msu_types, DFG->n_msu_types) != 0) {
        log_error("Error instantiating MSU types found in %s",
                  filename);
        return -1;
    }

    if (instantiate_dfg_runtime(LOCAL_RUNTIME) != 0) {
        log_error("Error instantiating runtime %d from %s",
                  runtime_id, filename);
        return -1;
    }

    return 0;
}

int controller_address(struct sockaddr_in *addr) {
    if (DFG == NULL) {
        log_error("Runtime DFG not instantiated");
        return -1;
    }

    bzero(addr, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = DFG->global_ctl_ip;
    addr->sin_port = htons(DFG->global_ctl_port);

    return 0;
}

int local_runtime_id() {
    if (LOCAL_RUNTIME == NULL) {
        log_error("Runtime DFG not instantiated");
        return -1;
    }
    return LOCAL_RUNTIME->id;
}

int local_runtime_port() {
    if (LOCAL_RUNTIME == NULL) {
        log_error("Runtime DFG not instantiated");
        return -1;
    }
    return LOCAL_RUNTIME->port;
}

uint32_t local_runtime_ip() {
    if (LOCAL_RUNTIME == NULL) {
        log_error("Runtime DFG not implemented");
        return -1;
    }
    return LOCAL_RUNTIME->ip;
}

struct dedos_dfg *get_dfg() {
    return DFG;
}

void free_runtime_dfg() {
    log_info("Freeing runtime DFG");
    free_dfg(DFG);
    //TODO: This shouldn't be in this function
    destroy_msu_types();
}
