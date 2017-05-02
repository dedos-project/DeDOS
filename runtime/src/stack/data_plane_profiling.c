#include "data_plane_profiling.h"
#include <assert.h>

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
    log_error("TODO: Init data profile in memory log");
    return 0;
}
