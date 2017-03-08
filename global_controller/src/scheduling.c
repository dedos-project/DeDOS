#include "dfg.h"
#include "scheduling.h"

int find_placement(int *runtime_sock, int *thread_id) {

    struct dfg_config *dfg_config_g = get_dfg();
    *runtime_sock = NULL;
    *thread_id = NULL;

    /**
     * For now we always place the new msu on a new thread. Thus this thread must be created on
     * the target runtime
     */
    int m;
    for (m = 0; m < MAX_RUNTIMES; m++) {
        if (dfg_config_g->runtimes[m]->num_threads < dfg_config_g->runtimes[m]->num_cores) {
            *runtime_sock = dfg_config_g->runtimes[m]->sock;
            *thread_id    = dfg_config_g->runtimes[m]->num_threads;
            return 0;
        }
    }

    if (*runtime_sock == NULL && *thread_id == NULL) {
        debug("ERROR: %s", "could not find runtime or thread to place new clone");
        return -1;
    }
 }
