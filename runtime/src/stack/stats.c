#include "stats.h"
#include "statistics.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>


// If STATLOG is defined to be 0, these functions are defined as
// empty macros in the header file
#if STATLOG

#ifndef LOG_STAT_INTERNALS
#define LOG_STAT_INTERNALS 0
#endif

/** The maximum number of statistics that can be gathered about
 * a single item before the structure is written to disk */
#define MAX_STATS 2048

/** The maximum number of items that can be gathered within
 * a single statistic */
#define MAX_ITEM_ID 64

/** The type of clock being used for timestamps */
#define CLOCK_ID CLOCK_MONOTONIC

// The following enable the user to turn on or off logging of specific stats from the Makefile
#ifndef LOG_QUEUE_LEN
#define LOG_QUEUE_LEN 1
#endif

#ifndef LOG_ITEMS_PROCESSED
#define LOG_ITEMS_PROCESSED 1
#endif

#ifndef LOG_MSU_FULL_TIME
#define LOG_MSU_FULL_TIME 1
#endif

#ifndef LOG_MSU_INTERNAL_TIME
#define LOG_MSU_INTERNAL_TIME 1
#endif

#ifndef LOG_MSU_INTERIM_TIME
#define LOG_MSU_INTERIM_TIME 1
#endif

#ifndef LOG_N_CONTEXT_SWITCH
#define LOG_N_CONTEXT_SWITCH 1
#endif

#ifndef LOG_BYTES_RECEIVED
#define LOG_BYTES_RECEIVED 1
#endif

#ifndef LOG_BYTES_SENT
#define LOG_BYTES_SENT 1
#endif

#ifndef LOG_GATHER_THREAD_STATS
#define LOG_GATHER_THREAD_STATS 1
#endif

struct stat_type {
    enum stat_id stat_id; /**< Stat ID as defined in stats.h */
    int enabled;          /**< If 1, logging for this item is enabled */
    int n_item_ids;       /**< Number of unique items that can be logged with this statistic
                               (e.g. # of MSUS) */
    int max_stats;        /**< Maximum number of statistics that can be held in memory at a time */
    char *stat_format;    /**< Format for printf */
    char *stat_name;      /**< Name to output for this statistic */
};

#define MAX_STAT 100000

/**
 * This structure defines the format and size of the statistics being logged
 * NOTE: Entries *MUST* be in the same order as the enumerator in stats.h!
 */
struct stat_type stat_types[] = {
   {QUEUE_LEN,           LOG_QUEUE_LEN,           32, MAX_STAT, "%02.0f",  "MSU_QUEUE_LENGTH"},
   {ITEMS_PROCESSED,     LOG_ITEMS_PROCESSED,     32, MAX_STAT, "%03.0f",  "ITEMS_PROCESSED"},
   {MSU_FULL_TIME,       LOG_MSU_FULL_TIME,       32, MAX_STAT, "%0.9f",   "MSU_FULL_TIME"},
   {MSU_INTERNAL_TIME,   LOG_MSU_INTERNAL_TIME,   32, MAX_STAT, "%0.9f",   "MSU_INTERNAL_TIME"},
   {MSU_INTERIM_TIME,    LOG_MSU_INTERIM_TIME,    32, MAX_STAT, "%0.9f",   "MSU_INTERIM_TIME"},
   {N_CONTEXT_SWITCH,    LOG_N_CONTEXT_SWITCH,    8,  MAX_STAT, "%3.0f",   "N_CONTEXT_SWITCHES"},
   {BYTES_SENT,          LOG_BYTES_SENT,          1,  MAX_STAT, "%06.0f",  "BYTES_SENT"},
   {BYTES_RECEIVED,      LOG_BYTES_RECEIVED,      1,  MAX_STAT, "%06.0f",  "BYTES_RECEIVED"},
   {GATHER_THREAD_STATS, LOG_GATHER_THREAD_STATS, 8,  MAX_STAT, "%0.9f",   "GATHER_THREAD_STATS"}
};

#define N_STAT_TYPES sizeof(stat_types) / sizeof(struct stat_type)
#define MAX_STAT_SAMPLES 128

/**
 * The internal statistics structure where stats are aggregated
 * before being written to disk.
 * One per statistic-item.
 */
struct item_stats
{
    unsigned int item_id;     /**< A unique identifier for the item being logged */
    int n_stats;              /**< The number of stats currently aggregated in the struct */
    int rolled_over;          /**< Whether the stats structure has rolled over at least once */
    struct timed_stat *stats; /**< Timestamp and data for each gathered statistic */
    int last_sample_index;    /**< Index at which the stat was sampled last */
    struct timed_stat sample[MAX_STAT_SAMPLES]; /**< Filled with stat samples on call to
                                                     sample_stats() */
    pthread_mutex_t mutex;
};

/**
 * Contains all items being gathered for a single statistic
 */
struct dedos_stats{
    enum stat_id stat_id;
    struct item_stats *item_stats;
};

/** All statistics are saved in this instance until written to disk */
struct dedos_stats saved_stats[N_STAT_TYPES];

/** The time at which DeDos started (in seconds) */
time_t start_time_s;
/** Mutex to make sure log is written to by one thread at a time */
pthread_mutex_t log_mutex;
/** The file to which stats are written */
FILE *statlog;

/** Gets the amount of time that has elapsed since logging started .
 * @param *t the elapsed time is output into this variable
 *
 */
static inline void get_elapsed_time(struct timespec *t) {
    clock_gettime(CLOCK_ID, t);
    t->tv_sec -= start_time_s;
}

/** Locks an item_stats structure, printing an error message on failure
 * @param *item the item_stats structure to lock
 * @return 0 on success, -1 on error
 */
static inline int lock_item(struct item_stats *item) {
    if ( pthread_mutex_lock(&item->mutex) != 0) {
        log_error("Error locking mutex for an item with ID %d", item->item_id);
        return -1;
    }
    return 0;
}

/** Unlocks an item_stats structure, printing an error message on failure
 * @param *item the item_stats structure to unlock
 * @return 0 on success, -1 on error
 */
static inline int unlock_item(struct item_stats *item) {
    if ( pthread_mutex_unlock(&item->mutex) != 0) {
        log_error("Error locking mutex for an item with ID %d", item->item_id);
        return -1;
    }
    return 0;
}

/** Writes gathered statistics for an individual item to the log file.
 * @param stat_id ID of the statistic to be logged
 * @param item_id ID of the specific item being logged (must be less than MAX_ITEM_ID)
 */
void flush_item_to_log(enum stat_id stat_id, unsigned int item_id) {
    struct item_stats *item = &saved_stats[stat_id].item_stats[item_id];

    lock_item(item);

    struct stat_type *stat_type = &stat_types[stat_id];
    int n_stats = item->n_stats;
    if (item->rolled_over)
        n_stats = stat_type->max_stats;

    char label[64];
    sprintf(label, "%s:%02d:", stat_type->stat_name, item_id);
    size_t label_size = strlen(label);

    for (int i=0; i<n_stats; i++) {
        fwrite(label, 1, label_size, statlog);
        fprintf(statlog, "%05ld.%09ld:", item->stats[i].time.tv_sec, item->stats[i].time.tv_nsec);
        fprintf(statlog, stat_type->stat_format, item->stats[i].stat);
        fwrite("\n", 1, 1, statlog);
    }

    unlock_item(item);
}

/** Writes all gathered statistics to the log file
 */
void flush_all_stats_to_log(void) {
    for (int i=0; i<N_STAT_TYPES; i++) {
        if (!stat_types[i].enabled)
            continue;
        for (int j=0; j<stat_types[i].n_item_ids; j++) {
            flush_item_to_log(i, j);
        }
    }
}

/** Adds the elapsed time since the previous aggregate_start_time(stat_id, item_id)
 * to the log.
 * @param stat_id ID for the statistic being logged
 * @param item_id ID for the item to which the statistic refers (< MAX_ITEM_ID)
 */
void aggregate_end_time(enum stat_id stat_id, unsigned int item_id) {
    struct stat_type *stat_type = &stat_types[stat_id];
    if (!stat_type->enabled) {
        return;
    }

    struct timespec newtime;
    get_elapsed_time(&newtime);
    struct item_stats *item = &saved_stats[stat_id].item_stats[item_id];

    lock_item(item);

    time_t timediff_s = newtime.tv_sec - item->stats[item->n_stats].time.tv_sec;
    long timediff_ns = newtime.tv_nsec - item->stats[item->n_stats].time.tv_nsec;
    item->stats[item->n_stats].stat  = (double)timediff_s + ((double)timediff_ns/(1000000000.0));
    item->n_stats++;
    if (item->n_stats == stat_type->max_stats) {
        log_warn("Stats for type %s rolling over", stat_type->stat_name);
        item->n_stats = 0;
        item->rolled_over = 1;
    }

    unlock_item(item);
}

/** Starts a measurement of how much time elapses. This information is not added
 * to the log until the next call to aggregate_end_time(stat_id, item_id)
 * @param stat_id ID for the statistic being logged
 * @param item_id ID for the item to which the statistic refers (< MAX_ITEM_ID)
 */
void aggregate_start_time(enum stat_id stat_id, unsigned int item_id) {
    struct stat_type *stat_type = &stat_types[stat_id];
    if ( !stat_type->enabled ) {
        return;
    }
    struct item_stats *item = &saved_stats[stat_id].item_stats[item_id];
    lock_item(item);
    get_elapsed_time(&item->stats[item->n_stats].time);
    unlock_item(item);
}

int get_time_diff_ms(struct timespec *t1, struct timespec *t2) {
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
void periodic_aggregate_end_time(enum stat_id stat_id, unsigned int item_id, int period_ms) {
    struct stat_type *stat_type = &stat_types[stat_id];
    if ( !stat_type->enabled ) {
        return;
    }

    struct timespec newtime;
    get_elapsed_time(&newtime);
    struct timespec curr_time;
    get_elapsed_time(&curr_time);

    struct item_stats *item = &saved_stats[stat_id].item_stats[item_id];

    lock_item(item);
    int do_log = get_time_diff_ms(&item->stats[item->n_stats-1].time, &curr_time) > period_ms;
    unlock_item(item);

    if (do_log) {
        aggregate_end_time(stat_id, item_id);
    }
}

/** Adds a single stastic for a single item to the log.
 * @param stat_id ID for the statistic being logged
 * @param item_id ID for the item to which the statistic refers (< MAX_ITEM_ID)
 * @param stat The specific statistic being logged
 * @param relog Whether to re-log a stat if it has not changed since the previous log
 */
void aggregate_stat(enum stat_id stat_id, unsigned int item_id, double stat, int relog) {
    struct stat_type *stat_type = &stat_types[stat_id];
    if ( !stat_types->enabled ) {
        return;
    }
    struct item_stats *item = &saved_stats[stat_id].item_stats[item_id];

    lock_item(item);

    int last_idx = item->n_stats % stat_type->max_stats;

    if ( relog || (item->stats[last_idx].stat != stat ) ) {
        get_elapsed_time(&item->stats[item->n_stats].time);
        item->stats[item->n_stats].stat = stat;
        item->n_stats++;
        if (item->n_stats == stat_type->max_stats) {
            log_warn("Stats for type %s rolling over", stat_type->stat_name);
            item->n_stats = 0;
            item->rolled_over = 1;
        }
    }

    unlock_item(item);
}

/** Aggregates a statistic, but only if period_ms milliseconds have passed.
 * @param stat_id ID for the statistic being logged
 * @param item_id ID for the item to which the statistic refers (< MAX_ITEM_ID)
 * @param stat The value of the statistic to be logged
 * @param period_ms The number of milliseconds that must have passed for this to be logged again*/
void periodic_aggregate_stat(enum stat_id stat_id, unsigned int item_id, double stat, int period_ms) {
    if (stat_types[stat_id].enabled == 0)
        return;

    struct item_stats *item = &saved_stats[stat_id].item_stats[item_id];
    struct timespec curr_time;
    get_elapsed_time(&curr_time);

    lock_item(item);
    int index = item->n_stats-1;
    int do_log = 1;
    if (index >= 0){
        do_log = get_time_diff_ms(&item->stats[item->n_stats-1].time, &curr_time) > period_ms;
    }
    unlock_item(item);
    if (do_log) {
        aggregate_stat(stat_id, item_id, stat, 1);
    }
}


static inline int time_cmp(struct timespec *t1, struct timespec *t2) {
    return t1->tv_sec > t2->tv_sec ? 1 :
            ( t1->tv_sec < t2->tv_sec ? -1 :
             ( t1->tv_nsec > t2->tv_nsec ? 1 :
               ( t1->tv_nsec < t2->tv_nsec ? -1 : 0 ) ) );
}

static int find_time_index(struct timed_stat *stats, int max_stats,
                           struct timespec *time, int start, int stop) {
    // FIXME: If max_stats is too low, or time is too long ago, there will be a race condition here
    int write_index = stop;
    int last_index = start;

    struct timespec *last_time = &stats[last_index].time;

    int delta = time_cmp(time, last_time);

    int i = last_index;
    int stop_index = write_index - delta;
    int n_searched = 0;

    // TODO: This is doing a linear search for the time. Could improve if bottlenecked.
    while ( time_cmp(time, &stats[i].time) != -delta ) {
        n_searched++;
        if ( i == stop_index ) {
            return -1;
        } else {
            i += delta;
            if ( i < 0 || i == max_stats ) {
                i %= max_stats;
            }
        }
    }
    log_custom(LOG_STAT_INTERNALS, "Found time at index %d (searched %d)", i, n_searched);
    return i;
}

static int sample_stats_between(struct timed_stat *stats, int max_stats, int start_i, int end_i,
                                int sample_size, struct timed_stat *sample) {

    log_custom(LOG_STAT_INTERNALS, "Sampling %d stats between %d-%d",sample_size,  start_i, end_i);

    double interval = (double)((end_i - start_i) % max_stats) / (sample_size - 1);

    double sample_i = (double)end_i - (interval * (sample_size - 1));
    for ( int i=0; i < sample_size; i++ ){
        sample[i] = stats[(int)sample_i];
        sample_i += interval;
        if ( sample_i >= max_stats ) {
            sample_i = (double) ((int)sample_i % max_stats);
        }
    }

    return 0;
}

struct timed_stat *sample_stats(enum stat_id stat_id, int item_id, time_t duration, int n_stats) {
    struct stat_type *stat_type = &stat_types[stat_id];
    struct item_stats *item = &saved_stats[stat_id].item_stats[item_id];

    if ( n_stats > MAX_STAT_SAMPLES ) {
        log_error("Requested sample size (%d) is bigger than maximum size (%d)",
                  n_stats, MAX_STAT_SAMPLES);
    }

    lock_item(item);
    int write_index = item->n_stats - 1;
    struct timespec cur_time = item->stats[write_index].time;
    unlock_item(item);

    if ( !item->rolled_over && n_stats > write_index ) {
        log_error("Requested sample size (%d) is larger than collected statistics (%d)",
                  n_stats, item->n_stats);
    }

    struct timespec start_time = cur_time;
    start_time.tv_sec -= duration;

    int start_index = find_time_index(item->stats, stat_type->max_stats,
            &start_time, item->last_sample_index, write_index);

    if ( start_index < 0 ) {
        log_error("Could not find starting index for sample of length %d", (int)duration);
        return NULL;
    }

    log_custom(LOG_STAT_INTERNALS, "Sampling %d/%d samples starting at time %d (currently %d)",
               n_stats,write_index - start_index, (int)start_time.tv_sec, (int)cur_time.tv_sec);


    int rtn = sample_stats_between(item->stats, stat_type->max_stats, start_index, write_index,
                                   n_stats, item->sample);
    if ( rtn < 0 ) {
        log_error("Error sampling stats");
        return NULL;
    }

    return item->sample;
}

/**
 * Opens the log file for statistics and initializes the stat structure
 * @param filename The filename to which logs are written
 */
void init_statlog(char *filename) {
    struct timespec start_time;
    clock_gettime(CLOCK_ID, &start_time);
    start_time_s = start_time.tv_sec;


    for (int i=0; i<N_STAT_TYPES; i++) {
        struct stat_type *stat_type = &stat_types[i];
        if ( stat_type->stat_id != i ) {
            log_error("Stat type %s at incorrect index (%d, not %d)! Disabling!!",
                      stat_type->stat_name, i, stat_type->stat_id);
            stat_type->enabled = 0;
        }

        if (stat_type->enabled) {
            struct item_stats *item_stats = malloc(stat_type->n_item_ids * sizeof(*item_stats));

            for (int j=0; j<stat_type->n_item_ids; j++) {
                struct item_stats *item = &item_stats[j];
                item->stats = calloc(stat_type->max_stats, sizeof(*item->stats));
                item->n_stats = 0;
                item->rolled_over = 0;
                int rtn = pthread_mutex_init(&item->mutex, NULL);
                if (rtn < 0) {
                    log_warn("Error initializing pthread_mutex for stat item %s(%d)",
                             stat_type->stat_name, j);
                }
            }
            saved_stats[i].item_stats = item_stats;

        }
    }
    if ( filename != NULL ) {
        statlog = fopen(filename, "w");
    }
}

/**
 * Flushes all stats to the log file, and closes the file
 */
void close_statlog() {
    log_info("Outputting stats to log. Do not quit");
    flush_all_stats_to_log();
    fclose(statlog);
    log_info("Finished outputting stats to log");
}

#endif

