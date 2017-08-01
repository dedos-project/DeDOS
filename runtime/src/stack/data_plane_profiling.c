#include "data_plane_profiling.h"
#include <assert.h>
#include <stdio.h>

#define REQUEST_ID_OFFSET 1000000

//#ifdef DATAPLANE_PROFILING
unsigned long int get_request_id() {
    static unsigned long int counter;
    unsigned long int ret_val = 0;

    if (counter == 0) {
        srand(time(NULL)); // should only be called once
    }

    float r = rand() % 10;
    if (r > dprof_tag_probability * 10.0) {
        //Do not tag the request, i.e. not track this
        return ret_val;
    }

    pthread_mutex_lock(&request_id_mutex);
    counter++;
    ret_val = counter + REQUEST_ID_OFFSET;
    pthread_mutex_unlock(&request_id_mutex);

    return ret_val;
}

int init_data_plane_profiling(void) {
    if (pthread_mutex_init(&request_id_mutex, NULL) != 0) {
        log_error("request id mutex init failed");
        return 1;
    }
    log_debug("Initialized mutex for dataplane request count");
    memset(&mem_dp_profile_log,'\0',sizeof(struct in_memory_profile_log));
    dprof_tag_probability = 1;
/*
    if (pthread_mutex_init(&mem_dp_profile_log.mutex, NULL) != 0)
    {
        log_error("In memory profile log mutex init failed");
        return 1;
    }
    mem_dp_profile_log.in_memory_entry_count = 0;
    mem_dp_profile_log.in_memory_entry_max_capacity = MAX_DATAPLANE_IN_MEMORY_LOG_ITEMS;
    log_debug("Initialized in memory profile log");
*/

    /* Skipping in memory log and directly write to log since its buffered */
    if (pthread_mutex_init(&fp_log_mutex, NULL) != 0) {
        log_error("FP log file mutex init failed");
        return 1;
    }

    // open file here and close on exit
    char *logfile = "dataplane_profile.log\0";
    pthread_mutex_lock(&fp_log_mutex);
    fp_log = fopen(logfile, "w");
    if (fp_log == NULL) {
        log_error("Can't open file: %s",logfile);
        pthread_mutex_unlock(&fp_log_mutex);
        return -1;
    }
    //TODO add setbuf options to buffer
    pthread_mutex_unlock(&fp_log_mutex);
    return 0;
}

int deinit_data_plane_profiling(void) {
    pthread_mutex_lock(&fp_log_mutex);
    fclose(fp_log);
    pthread_mutex_unlock(&fp_log_mutex);
    log_info("Closed dataprofile log file");
    return 0;
}

void clear_in_memory_profile_log(void) {
    memset(&mem_dp_profile_log.in_memory_entries,'\0',
            sizeof(char)*MAX_DATAPLANE_IN_MEMORY_LOG_ITEMS * MAX_DATAPLANE_LOG_ENTRY_LEN);
    mem_dp_profile_log.in_memory_entry_count = 0;
    mem_dp_profile_log.in_memory_entry_max_capacity = MAX_DATAPLANE_IN_MEMORY_LOG_ITEMS;
}

int dump_profile_logs(char *logfile) {
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
//#endif
