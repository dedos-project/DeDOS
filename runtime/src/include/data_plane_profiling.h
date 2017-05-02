#ifndef DATA_PLANE_PROFILING_H_
#define DATA_PLANE_PROFILING_H_
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "logging.h"
/** Number of entries per item that can be logged from entry to exit */
#define MAX_DATAPLANE_ITEMS 64
#define MAX_DATAPLANE_LOG_ENTRY_LEN 512
#define CLOCK_ID CLOCK_MONOTONIC


pthread_mutex_t request_id_mutex;

typedef enum {
    ENQUEUE = 0,
    DEQUEUE,
    REMOTE_SEND, //When sent over TCP conn b/w runtimes
    REMOTE_RECV, //When recevied over TCP conn b/w runtimes
    DEDOS_ENTRY,
    DEDOS_EXIT
} enum_dataplane_op_id;

static const char *enum_dataplane_op_name[] = {
    "ENQUEUE",
    "DEQUEUE",
    "REMOTE_SEND", //When sent over TCP conn b/w runtimes
    "REMOTE_RECV", //When recevied over TCP conn b/w runtimes
    "DEDOS_ENTRY",
    "DEDOS_EXIT"
};

struct dataplane_profile_info {
    /** Unique id to be assigned to each packet at entry point of dedos */
    int dp_id;
    /** Current count of dataplane log entries (dp_entry_count < MAX_DATAPLANE_ITEMS) */
    int dp_entry_count;
    /** 2d char array to hold MAX_DATAPLANE_ITEMS logs items with len MAX_DATAPLANE_LOG_ENTRY_LEN */
    char dp_log_entries[MAX_DATAPLANE_ITEMS][MAX_DATAPLANE_LOG_ENTRY_LEN];
};

static void inline log_dp_event(int msu_id, enum_dataplane_op_id dataplane_op_id,
    struct dataplane_profile_info *dp_profile_info)
{
    pthread_t self_tid = pthread_self();
    struct timespec cur_timestamp;
    long int ts;
    clock_gettime(CLOCK_ID, &cur_timestamp);
    ts = (cur_timestamp.tv_sec * 1000 * 1000) + (cur_timestamp.tv_nsec / 1000);
    snprintf(dp_profile_info->dp_log_entries[dp_profile_info->dp_entry_count],
            MAX_DATAPLANE_LOG_ENTRY_LEN,
            "%ld, %lu, %d, %d, %d, %s",
            ts, self_tid, dp_profile_info->dp_id, dp_profile_info->dp_entry_count,
                msu_id, enum_dataplane_op_name[dataplane_op_id]);
    log_profile("%s",dp_profile_info->dp_log_entries[dp_profile_info->dp_entry_count]);
    dp_profile_info->dp_entry_count++;
}

int init_data_plane_profiling(void);
int get_request_id(void);

// #ifdef DATAPLANE_PROFILING
// /* declare everything here */
// #endif /* DATAPLANE_PROFILING */


#endif /* DATA_PLANE_PROFILING_H_ */
