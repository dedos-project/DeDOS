#ifndef DATA_PLANE_PROFILING_H_
#define DATA_PLANE_PROFILING_H_
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "logging.h"
/** Number of entries per item that can be logged from entry to exit */
#define MAX_DATAPLANE_IN_MEMORY_LOG_ITEMS 1048576 /* for in memory log accross runtime */
#define MAX_DATAPLANE_ENTRIES_PER_ITEM 32 /* 32 max hops */
#define MAX_DATAPLANE_LOG_ENTRY_LEN 512
#define CLOCK_ID CLOCK_MONOTONIC

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
    /** Hop count on logical flow for this item inside dedos system */
    int dp_seq_count;
    /** 2d char array to hold MAX_DATAPLANE_ITEMS logs items with len MAX_DATAPLANE_LOG_ENTRY_LEN */
    char dp_log_entries[MAX_DATAPLANE_ENTRIES_PER_ITEM][MAX_DATAPLANE_LOG_ENTRY_LEN];
};

struct in_memory_profile_log {
    /** Mutex, multiple threads update this */
    pthread_mutex_t mutex;
    /** Current number of entries stored */
    int in_memory_entry_count;
    /** Capacity for entries */
    int in_memory_entry_max_capacity;
    /** 2d char of hold profile log */
    char in_memory_entries[MAX_DATAPLANE_IN_MEMORY_LOG_ITEMS][MAX_DATAPLANE_LOG_ENTRY_LEN];
};

int dump_profile_logs(char *logfile);
int init_data_plane_profiling(void);
void clear_in_memory_profile_log(void);
int get_request_id(void);

/***GLOBALS***/
pthread_mutex_t request_id_mutex;
struct in_memory_profile_log mem_dp_profile_log;

static void copy_queue_item_dp_data(struct dataplane_profile_info *dp_profile_info){
    log_debug("Current entry count in memory log: %d",mem_dp_profile_log.in_memory_entry_count);
    log_debug("Current entry count in item log: %d",dp_profile_info->dp_entry_count);
    log_debug("Capacity of in memory log: %d",mem_dp_profile_log.in_memory_entry_max_capacity);

    if(mem_dp_profile_log.in_memory_entry_count + dp_profile_info->dp_entry_count >= mem_dp_profile_log.in_memory_entry_max_capacity){
        log_warn("Overflow...in memory profile log...dump before continuing...");
        dump_profile_logs(NULL);
        log_warn("Dumped in memory profile log....");
    }

    pthread_mutex_lock(&mem_dp_profile_log.mutex);
    memcpy(&mem_dp_profile_log.in_memory_entries[mem_dp_profile_log.in_memory_entry_count], 
          dp_profile_info->dp_log_entries, sizeof (char) * dp_profile_info->dp_entry_count * MAX_DATAPLANE_LOG_ENTRY_LEN);
    mem_dp_profile_log.in_memory_entry_count += dp_profile_info->dp_entry_count;
    pthread_mutex_unlock(&mem_dp_profile_log.mutex);

    log_debug("Current entry count in memory log after copy: %d",mem_dp_profile_log.in_memory_entry_count);
    log_debug("Queue item entries: ");
#if DEBUG == 1
    int i=0;
    for(i=0; i < dp_profile_info->dp_entry_count; i++){
        printf("%s\n",dp_profile_info->dp_log_entries[i]);
    }
    log_debug("InMemory item entries:");
    for(i=0; i < mem_dp_profile_log.in_memory_entry_count; i++){
        printf("%s\n",mem_dp_profile_log.in_memory_entries[i]);
    }
//        log_debug("Test profile log mem dump");
//        dump_profile_logs(NULL);
#endif
}

static void log_dp_event(int msu_id, enum_dataplane_op_id dataplane_op_id,
    struct dataplane_profile_info *dp_profile_info)
{
    pthread_t self_tid = pthread_self();
    struct timespec cur_timestamp;
    long int ts;
    clock_gettime(CLOCK_ID, &cur_timestamp);
    ts = (cur_timestamp.tv_sec * 1000 * 1000) + (cur_timestamp.tv_nsec / 1000);
    if(dp_profile_info->dp_entry_count < MAX_DATAPLANE_ENTRIES_PER_ITEM){
        if(dataplane_op_id == REMOTE_RECV){
            dp_profile_info->dp_seq_count++;
        }
        snprintf(dp_profile_info->dp_log_entries[dp_profile_info->dp_entry_count],
                MAX_DATAPLANE_LOG_ENTRY_LEN,
                "%ld, %lu, %d, %d, %d, %d, %s",
                ts, self_tid, dp_profile_info->dp_id,dp_profile_info->dp_seq_count,
                dp_profile_info->dp_entry_count,
                    msu_id, enum_dataplane_op_name[dataplane_op_id]);
        log_profile("%s",dp_profile_info->dp_log_entries[dp_profile_info->dp_entry_count]);
        dp_profile_info->dp_entry_count++;
        dp_profile_info->dp_seq_count++;
    } else {
        log_error("Overflow in item profile entry count: %d",dp_profile_info->dp_entry_count);
    }
    //log to in memory log if we see DEDOS_EXIT event, by coping the whole dp_log_entries
    if(dataplane_op_id == DEDOS_EXIT){
        copy_queue_item_dp_data(dp_profile_info);
    } //if(dataplane_op_id == DEDOS_EXIT)
}

// #ifdef DATAPLANE_PROFILING
// /* declare everything here */
// #endif /* DATAPLANE_PROFILING */


#endif /* DATA_PLANE_PROFILING_H_ */
