/**
 * @file: runtime_dfg.c
 * Interactions with the global dfg from an individual runtime's perspective
 */
#include "runtime_dfg.h"

/** Static (global) variable for accessing a lodaed Dfg */
static struct dedos_dfg *DFG = NULL;
/** Static (global) variable holding this runtime's dfg */
static struct dfg_runtime *LOCAL_RUNTIME = NULL;

/**
 * Initializes the DFG as loaded from a JSON file, and sets
 * the global variables such that the DFG and runtime can be accessed
 * @param filename File from which to load the json DFG
 * @param runtime_id Idenitifer for this runtime
 * @return 0 on success, -1 on error
 */
int init_runtime_dfg(char *filename, int runtime_id) {
    
    if (DFG != NULL) {
        log_error("Runtime DFG already instantiated!");
        return -1;
    }

    DFG = load_dfg(filename);
    if (DFG == NULL) {
        log_error("Error initializing DFG");
        return -1;
    }

    LOCAL_RUNTIME = get_dfg_runtime(DFG, runtime_id);
    if (LOCAL_RUNTIME == NULL) {
        log_error("Error finding runtime %d in DFG %s", 
                  runtime_id, filename);
        return -1;
    }
    return 0;
}

/**
 * Gets the sockaddr associated with the global controller
 * @param addr Output parameter to be filled with controller's ip and port
 * @return 0 on success, -1 on error
 */
int controller_address(struct sockaddr_in *addr) {
    if (DFG == NULL) {
        log_error("Runtime DFG not instantiated");
        return -1;
    }

    bzero(addr, sizeof(*addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, DFG->controller_ip, &addr->sin_addr);
    addr.sin_port = htons(DFG->controller_port);

    return 0;
}

//TODO: local_runtime_id()

int local_runtime_port() {
    if (DFG == NULL) {
        log_error("Runtime DFG not instantiated");
        return -1;
    }
    return LOCAL_RUNTIME->port;
}

/**
 * Gets the loaded DFG
 * @return loaded DFG or NULL if not initialized
 */
struct dedos_dfg *get_dfg() {
    return DFG;
}
