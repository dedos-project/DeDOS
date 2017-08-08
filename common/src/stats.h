/**
 * stats.h
 *
 * Code related to the gathering of statistics on the runtime,
 * and the definition of the statistics that are passed to the controller
 */
#pragma once

#include <time.h>
#include <pthread.h>

#ifndef COLLECT_STATS
/** Whether or not to enable stat collection at all. System may be unstable without it. */
#define COLLECT_STATS 1
#endif

/** The maximum number of items that may be collected within each statistic */
#define MAX_STAT_ITEM_IDS 32

/** When downsampling statistics, the maximum number of samples that may be collected */
#define MAX_STAT_SAMPLES 128

/**
 * There must be a unique identifier for each statistic that is gathered.
 */
enum stat_id{
    QUEUE_LEN,
    ITEMS_PROCESSED,
    MSU_FULL_TIME,
    MSU_INTERNAL_TIME,
    MSU_INTERIM_TIME,
    MEMORY_ALLOCATED,
    N_CONTEXT_SWITCH,
    BYTES_SENT,
    BYTES_RECEIVED,
    GATHER_THREAD_STATS,
    N_STATES,
};


/**
 * Structure to hold a single timestamped statistic
 */
struct timed_stat {
    double stat;            /**< The statistic being collected */
    struct timespec time;   /**< The time at which the statistic was collected */
};

/**
 * Structure to hold a sample of timestamped statistics
 */
struct stat_sample {
    enum stat_id stat_id;     /**< ID for the statistic itself */
    int item_id;              /**< ID for the item to which the stat refers */
    struct timespec cur_time; /**< The time at which the statistics were sampled */
    int n_stats;              /**< The number of statistics that have been sampled */
    struct timed_stat *stats; /**< The statistics themselved */
};


#if COLLECT_STATS
/** Gets the amount of time that has elapsed since logging started*/
void get_elapsed_time(struct timespec *t) ;

/** Writes all gathered statistics to the log file*/
void flush_all_stats_to_log(void);

/** Adds the elapsed time as a statistic to the log (must follow aggregate_start_time()) */
void aggregate_end_time(enum stat_id stat_id, unsigned int item_id);

/** Starts a measurement of how much time elapses.
 * (the next call to aggregate_end_time() will add this stat to the log)
 * */
void aggregate_start_time(enum stat_id stat_id, unsigned int item_id);

/** Adds elapsed time as a statistic if period_ms milliseconds have passed */
void periodic_aggregate_end_time(enum stat_id stat_id, unsigned int item_id, int period_ms);

/** Adds a single statistic for a single item to the log */
void aggregate_stat(enum stat_id stat_id, unsigned int item_id, double stat, int relog);

/** Aggregates a statistic, but only if period_ms milliseconds have passed */
void periodic_aggregate_stat(enum stat_id stat_id, unsigned int item_id,
                             double stat, int period_ms);

/** Flushes all statistics to the log file and then closes the file */
void close_statlog();

/** Opens the log file for statistics and initializes the stat structure */
void init_statlog(char *filename);

int sample_stats(enum stat_id stat_id, time_t duration, int sample_size,
                 struct stat_sample *sample);

/** Access the last-entered value for a statistic.*/
double get_last_stat(enum stat_id stat_id, unsigned int item_id );

/** Adds a value to a stastic for a single item.*/
void increment_stat(enum stat_id stat_id, unsigned int item_id, double value);

#else

#define get_elapsed_time(...)
#define flush_stat_to_log(...)
#define flush_all_stats_to_log(...)
#define aggregate_end_time(...)
#define aggregate_start_time(...)
#define aggregate_stat(...)
#define close_statlog(...)
#define init_statlog(...)
#define sample_stats(...) 0
#define get_last_stat(...) 0
#define increment_stat(...)

#endif
