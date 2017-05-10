#include "stats.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>


// If STATLOG is defined to be 0, these functions are defined as
// empty macros in the header file
#if STATLOG

/** The maximum number of statistics that can be gathered about
 * a single item before the structure is written to disk */
#define MAX_STATS 2048

/** The maximum number of items that can be gathered within
 * a single statistic */
#define MAX_ITEM_ID 64

/** The minumum amount of time that must pass before statistics
 * are written to disk */
#define FLUSH_TIME_S 5

/** The type of clock being used for timestamps */
#define CLOCK_ID CLOCK_MONOTONIC

#ifndef LOG_SELF_TIME
#define LOG_SELF_TIME 1
#endif

#ifndef LOG_QUEUE_LEN
#define LOG_QUEUE_LEN 1
#endif

#ifndef LOG_ITEMS_PROCESSED
#define LOG_ITEMS_PROCESSED 1
#endif

#ifndef LOG_FULL_MSU_TIME
#define LOG_FULL_MSU_TIME 1
#endif

#ifndef LOG_MSU_INTERNAL_TIME
#define LOG_MSU_INTERNAL_TIME 1
#endif

#ifndef LOG_MSU_INTERIM_TIME
#define LOG_MSU_INTERIM_TIME 1
#endif

#ifndef LOG_N_SWAPS
#define LOG_N_SWAPS 1
#endif

#ifndef LOG_CPU_TIME
#define LOG_CPU_TIME 1
#endif

#ifndef LOG_THREAD_LOOP_TIME
#define LOG_THREAD_LOOP_TIME 1
#endif

#ifndef LOG_MESSAGES_SENT
#define LOG_MESSAGES_SENT 1
#endif

#ifndef LOG_MESSAGES_RECEIVED
#define LOG_MESSAGES_RECEIVED 1
#endif

#ifndef LOG_BYTES_RECEIVED
#define LOG_BYTES_RECEIVED 1
#endif

#ifndef LOG_BYTES_SENT
#define LOG_BYTES_SENT 1
#endif

/**
 * Defines which statistics are gathered.
 * Each entry corresponds to a single enumerator.
 * If an entry is set to 0, the call to log is immediately exited
 */
int log_mask[] = {
    LOG_SELF_TIME,
    LOG_QUEUE_LEN,
    LOG_ITEMS_PROCESSED,
    LOG_FULL_MSU_TIME,
    LOG_MSU_INTERNAL_TIME,
    LOG_MSU_INTERIM_TIME,
    LOG_N_SWAPS,
    LOG_CPU_TIME,
    LOG_THREAD_LOOP_TIME,
    LOG_MESSAGES_SENT,
    LOG_MESSAGES_RECEIVED,
    LOG_BYTES_SENT,
    LOG_BYTES_RECEIVED
};

/**
 * Defines the format in which the corresponding statistic is
 * output with sprintf
 */
char *stat_format[] = {
    "%0.9f",  // Self time
    "%02.0f", // Queue length
    "%03.0f", // Items processed
    "%0.9f",  // Full MSU time
    "%0.9f",  // Internal MSU time
    "%0.9f",  // Interim MSU time
    "%03.0f", // Number of non-voluntary context switches
    "%03.9f", // Elasped thread-specific CPU time
    "%01.9f", // Thread loop time
    "%03.0f", // N Messages sent
    "%03.0f", // N messages received
    "%06.0f", // Bytes sent
    "%06.0f", // Bytes received
};

/**
 * Defines the name that is prepended to the log line for each time
 * that the corresponding item is logged
 */
char *stat_name[] = {
    "_STAT_FLUSH_TIME",
    "MSU_QUEUE_LENGTH",
    "_ITEMS_PROCESSED",
    "___MSU_FULL_TIME",
    "__MSU_INNER_TIME",
    "MSU_INTERIM_TIME",
    "__N_THREAD_SWAPS",
    "PER_THREAD_CPU_T",
    "THREAD_LOOP_TIME",
    "_N_MESSAGES_SENT",
    "_N_MESSAGES_RCVD",
    "__NUM_BYTES_SENT",
    "__NUM_BYTES_RCVD"
};

/**
 * The internal statistics structure where stats are aggregated
 * before being written to disk.
 * One per statistic-item.
 */
struct item_stats
{
    time_t last_flush;      /**< The last time at which the stats were written to file */
    unsigned int item_id;   /**< A unique identifier for the item being logged */
    int n_stats;            /**< The number of stats currently aggregated in the struct */
    pthread_mutex_t mutex;  /**< A lock to ensure reading and writing do not overlap */
    struct timespec time[MAX_STATS]; /**< The time at which each statistic was gathered */
    double stat[MAX_STATS]; /**< The actual statistics gathered */
};

/**
 * Contains all items being gathered for a single statistic
 */
struct dedos_stats{
    enum stat_id stat_id;
    struct item_stats item_stats[MAX_ITEM_ID];
};

/** All statistics are saved in this instance until written to disk */
struct dedos_stats saved_stats[N_STAT_IDS];

/** The time at which DeDos started (in seconds) */
time_t start_time_s;
/** Mutex to make sure log is written to by one thread at a time */
pthread_mutex_t log_mutex;
/** The file to which stats are written */
FILE *statlog;

/** Gets the amount of time that has elapsed since logging started .
 * @param *t the elapsed time is output into this variable
 * */
void get_elapsed_time(struct timespec *t){
    clock_gettime(CLOCK_ID, t);
    t->tv_sec -= start_time_s;
}

/** Writes gathered statistics for an individual item to the log file.
 * @param stat_id ID of the statistic to be logged
 * @param item_id ID of the specific item being logged (must be less than MAX_ITEM_ID)
 */
void flush_item_to_log(enum stat_id stat_id, unsigned int item_id){
    aggregate_start_time(SELF_TIME, stat_id);
    struct item_stats *item = &saved_stats[(int)stat_id].item_stats[item_id];
    int n_to_write[item->n_stats];
    char to_write[item->n_stats][128];
    pthread_mutex_lock(&item->mutex);
    for (int i=0; i<item->n_stats; i++){
        int written = sprintf(to_write[i], "%s:%02d:%05ld.%09ld:",
                              stat_name[stat_id],
                              item_id,
                              item->time[i].tv_sec, item->time[i].tv_nsec);
        written += sprintf(to_write[i]+written, stat_format[(int)stat_id], item->stat[i]);
        n_to_write[i] = written;
    }
    struct timespec writetime;
    get_elapsed_time(&writetime);
    item->last_flush = writetime.tv_sec;

    item->time[0] = item->time[item->n_stats];
    item->stat[0] = item->stat[item->n_stats];
    int n_stats = item->n_stats;
    item->n_stats = 0;
    pthread_mutex_unlock(&item->mutex);

    pthread_mutex_lock(&log_mutex);
    for (int i=0; i<n_stats; i++){
        fwrite(to_write[i], sizeof(char), n_to_write[i], statlog);
        fwrite("\n", sizeof(char), 1, statlog);
    }
    pthread_mutex_unlock(&log_mutex);
    aggregate_end_time(SELF_TIME, stat_id);
}

/** Writes all gathered statistics to the log file if enough time has passed
 * @param force Forces the write to log even if enough time has not passed
 */
void flush_all_stats_to_log(int force){
    aggregate_start_time(SELF_TIME, N_STAT_IDS);
    struct timespec curtime;
    get_elapsed_time(&curtime);
    int did_log = 0;
    for (int i=N_STAT_IDS-1; i>=0; i--){
        for (int j=0; j<MAX_ITEM_ID; j++){
            struct item_stats *item = &saved_stats[i].item_stats[j];
            pthread_mutex_lock(&item->mutex);
            int do_log = item->n_stats > 0 &&
                    (curtime.tv_sec - item->last_flush > FLUSH_TIME_S || force);
            pthread_mutex_unlock(&item->mutex);
            if (do_log){
                flush_item_to_log(i, j);
                did_log = 1;
            }
        }
    }
    if (did_log)
        aggregate_end_time(SELF_TIME, N_STAT_IDS);

}

/** Adds the elapsed time since the previous aggregate_start_time(stat_id, item_id)
 * to the log.
 * @param stat_id ID for the statistic being logged
 * @param item_id ID for the item to which the statistic refers (< MAX_ITEM_ID)
 */
void aggregate_end_time(enum stat_id stat_id, unsigned int item_id){
    if (log_mask[stat_id] == 0)
        return;
    struct timespec newtime;
    get_elapsed_time(&newtime);
    struct item_stats *item = &saved_stats[(int)stat_id].item_stats[item_id];
    pthread_mutex_lock(&item->mutex);
    time_t timediff_s = newtime.tv_sec - item->time[item->n_stats].tv_sec;
    long timediff_ns = newtime.tv_nsec - item->time[item->n_stats].tv_nsec;
    item->stat[item->n_stats] = (double)timediff_s + ((double)timediff_ns/(1000000000.0));
    item->n_stats++;
    int flush = item->n_stats == MAX_STATS;
    pthread_mutex_unlock(&item->mutex);
    if (flush){
        flush_item_to_log(stat_id, item_id);
    }
}

/** Starts a measurement of how much time elapses. This information is not added
 * to the log until the next call to aggregate_end_time(stat_id, item_id)
 * @param stat_id ID for the statistic being logged
 * @param item_id ID for the item to which the statistic refers (< MAX_ITEM_ID)
 */
void aggregate_start_time(enum stat_id stat_id, unsigned int item_id){
    if (log_mask[stat_id] == 0)
        return;
    struct item_stats *item = &saved_stats[(int)stat_id].item_stats[item_id];
    pthread_mutex_lock(&item->mutex);
    get_elapsed_time(&item->time[item->n_stats]);
    pthread_mutex_unlock(&item->mutex);
}

int get_time_diff_ms(struct timespec *t1, struct timespec *t2){
    return (t2->tv_sec - t1->tv_sec) * 1000 + \
           (t2->tv_nsec - t1->tv_nsec) / 1000000;
}

/** Finishes a measurement of how much time has elapsed since the previous aggregate_start_time().
 * This function can safely be used with aggregate_start_time(), as subsequent calls to
 * aggregate_start_time() merely reset the timer, so the information will not be logged until
 * this function successfully executes.
 * @param stat_id ID for the statistic being logged
 * @param item_id ID for the item to which the statistic refers (< MAX_ITEM_ID)
 * @param period_ms The number of milliseconds that must have passed since the previous successful
 *                  call in order for this call to succeed
 */
void periodic_aggregate_end_time(enum stat_id stat_id, unsigned int item_id, int period_ms){
    if (log_mask[stat_id] == 0)
        return;

    struct timespec newtime;
    get_elapsed_time(&newtime);
    struct item_stats *item = &saved_stats[(int)stat_id].item_stats[item_id];
    pthread_mutex_lock(&item->mutex);

    struct timespec curr_time;
    get_elapsed_time(&curr_time);

    int do_log = get_time_diff_ms(&item->time[item->n_stats-1], &curr_time) > period_ms;

    if (do_log){
        time_t timediff_s = curr_time.tv_sec - item->time[item->n_stats].tv_sec;
        long timediff_ns = newtime.tv_nsec - item->time[item->n_stats].tv_nsec;
        item->stat[item->n_stats] = (double)timediff_s + ((double)timediff_ns/(1000000000.0));
        item->n_stats++;
    }
    int flush = item->n_stats == MAX_STATS;
    pthread_mutex_unlock(&item->mutex);
    if (flush)
        flush_item_to_log(stat_id, item_id);
}

/** Aggregates a statistic, but only if period_ms milliseconds have passed.
 * @param stat_id ID for the statistic being logged
 * @param item_id ID for the item to which the statistic refers (< MAX_ITEM_ID)
 * @param stat The value of the statistic to be logged
 * @param period_ms The number of milliseconds that must have passed for this to be logged again*/
void periodic_aggregate_stat(enum stat_id stat_id, unsigned int item_id, double stat, int period_ms){
    if (log_mask[stat_id] == 0)
        return;

    struct item_stats *item = &saved_stats[(int)stat_id].item_stats[item_id];
    pthread_mutex_lock(&item->mutex);

    struct timespec curr_time;
    get_elapsed_time(&curr_time);
    int do_log = get_time_diff_ms(&item->time[item->n_stats-1], &curr_time) > period_ms;
    if (do_log){
        item->time[item->n_stats] = curr_time;
        item->stat[item->n_stats] = stat;
        item->n_stats++;
    }
    int flush = item->n_stats == MAX_STATS;
    pthread_mutex_unlock(&item->mutex);
    if (flush){
        flush_item_to_log(stat_id, item_id);
    }
}


/** Adds a single stastic for a single item to the log.
 * @param stat_id ID for the statistic being logged
 * @param item_id ID for the item to which the statistic refers (< MAX_ITEM_ID)
 * @param stat The specific statistic being logged
 * @param relog Whether to re-log a stat if it has not changed since the previous log
 */
void aggregate_stat(enum stat_id stat_id, unsigned int item_id, double stat, int relog){
    if (log_mask[stat_id] == 0)
        return;
    struct item_stats *item = &saved_stats[(int)stat_id].item_stats[item_id];
    pthread_mutex_lock(&item->mutex);
    
    int i = item->n_stats == 0 ? 0 : item->n_stats-1;

    if ( (item->stat[i] != stat ) || relog){
        get_elapsed_time(&item->time[item->n_stats]);
        item->stat[item->n_stats] = stat;
        item->n_stats++;
    }
    int flush = item->n_stats == MAX_STATS;
    pthread_mutex_unlock(&item->mutex);
    if (flush){
        flush_item_to_log(stat_id, item_id);
    }
}

/**
 * Opens the log file for statistics and initializes the stat structure
 * @param filename The filename to which logs are written
 */
void init_statlog(char *filename){
    struct timespec start_time;
    clock_gettime(CLOCK_ID, &start_time);
    start_time_s = start_time.tv_sec;
    for (int i=0; i<N_STAT_IDS; i++){
        for (int j=0; j<MAX_ITEM_ID; j++){
            pthread_mutex_init(&saved_stats[i].item_stats[j].mutex, NULL );
            struct item_stats *item = &saved_stats[i].item_stats[j];
            item->last_flush = 0;
            item->n_stats = 0;
            memset(item->time, 0, sizeof(item->time));
            item->stat[0] = 0;
        }
    }
    statlog = fopen(filename, "w");
}

/**
 * Flushes all stats to the log file, and closes the file
 */
void close_statlog(){
    flush_all_stats_to_log(1);
    fclose(statlog);
}

#endif

