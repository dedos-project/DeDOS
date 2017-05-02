#include "data_plane_profiling.h"
#include <assert.h>

#ifdef DATAPLANE_PROFILING
int get_request_id(void){
    static int counter;
    int ret_val;

    pthread_mutex_lock(&request_id_mutex);
    counter++;
    ret_val = counter;
    pthread_mutex_unlock(&request_id_mutex);
    assert(ret_val > 0);
    return ret_val;
}

int init_data_plane_profiling(void){
    if (pthread_mutex_init(&request_id_mutex, NULL) != 0)
    {
        log_error("request id mutex init failed");
        return 1;
    }
    log_debug("Initialized mutex for dataplane request count");
    memset(&mem_dp_profile_log,'\0',sizeof(struct in_memory_profile_log));
    mem_dp_profile_log.in_memory_entry_max_capacity = MAX_DATAPLANE_IN_MEMORY_LOG_ITEMS;
    log_debug("Initialized in memory profile log");
    return 0;
}
#endif
