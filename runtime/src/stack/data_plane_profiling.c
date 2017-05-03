#include "data_plane_profiling.h"
#include <assert.h>
#include <stdio.h>

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
    if (pthread_mutex_init(&mem_dp_profile_log.mutex, NULL) != 0)
    {
        log_error("In memory profile log mutex init failed");
        return 1;
    }
    mem_dp_profile_log.in_memory_entry_count = 0;
    mem_dp_profile_log.in_memory_entry_max_capacity = MAX_DATAPLANE_IN_MEMORY_LOG_ITEMS;
    log_debug("Initialized in memory profile log");
    return 0;
}

void clear_in_memory_profile_log(void){
    memset(&mem_dp_profile_log.in_memory_entries,'\0',
            sizeof(char)*MAX_DATAPLANE_IN_MEMORY_LOG_ITEMS * MAX_DATAPLANE_LOG_ENTRY_LEN);
    mem_dp_profile_log.in_memory_entry_count = 0;
    mem_dp_profile_log.in_memory_entry_max_capacity = MAX_DATAPLANE_IN_MEMORY_LOG_ITEMS;
}

int dump_profile_logs(char *logfile){
    FILE *fp;
    char *backup_log_file = "dataplane_profile.log\0";
    if(!logfile){
        logfile = backup_log_file;
    }
    log_debug("dataplane profile log file: %s",logfile);
    int i;

    pthread_mutex_lock(&mem_dp_profile_log.mutex);

    fp = fopen(logfile, "a");
    if (fp == NULL) {
        log_error("Can't open file: %s",logfile);
        pthread_mutex_unlock(&mem_dp_profile_log.mutex);
        return -1;
    }
    for(i=0; i < mem_dp_profile_log.in_memory_entry_count; i++){
        fprintf(fp, "%s\n",mem_dp_profile_log.in_memory_entries[i]);
    }
    fclose(fp);
    clear_in_memory_profile_log();
    
    pthread_mutex_unlock(&mem_dp_profile_log.mutex);
    return 0;
}
#endif
