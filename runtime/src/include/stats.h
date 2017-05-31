#pragma once
#include <time.h>
#include <pthread.h>


#ifndef STATLOG
#define STATLOG 1
#endif
/**
 * There must be a unique identifier for each statistic that is gathered.
 */
enum stat_id{
    QUEUE_LEN,
    ITEMS_PROCESSED,
    MSU_FULL_TIME,
    MSU_INTERNAL_TIME,
    MSU_INTERIM_TIME,
    N_CONTEXT_SWITCH,
    BYTES_SENT,
    BYTES_RECEIVED,
    GATHER_THREAD_STATS,
};

#if STATLOG

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

#else

#define flush_stat_to_log(...)
#define flush_all_stats_to_log(...)
#define aggregate_end_time(...)
#define aggregate_start_time(...)
#define aggregate_stat(...)
#define close_statlog(...)
#define init_statlog(...)

#endif
