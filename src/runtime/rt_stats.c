/**
 * @file rt_stats.c
 *
 * Collecting statistics within the runtime
 */

#include "rt_stats.h"
#include "stats.h"
#include "logging.h"

#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

/** The clock used to mark the time at which events take place */
#define CLOCK_ID CLOCK_REALTIME_COARSE
#define DURATION_CLOCK_ID CLOCK_MONOTONIC

/** The time at which the runtime started */
static struct timespec stats_start_time;
/** Whether ::init_statistics has been called */
static bool stats_initialized = false;


/**
 * The internal statistics structure where stats are aggregated
 * One per statistic-item.
 */
struct stat_item
{
    unsigned int id;          /**< A unique identifier for the item being logged */
    int write_index;          /**< The write index in the circular buffer */
    bool rolled_over;         /**< Whether the stats structure has rolled over at least once */
    struct timed_stat *stats; /**< Timestamp and data for each gathered statistic */
    pthread_mutex_t mutex;    /**< Lock for changing stat item */
};

/**
 * The structure holding all items of a type of stats
 */
struct stat_type {
    enum stat_id id;          /**< Stat ID as defined in stats.h */
    bool enabled;             /**< If true, logging for this item is enabled */
    int max_stats;            /**< Maximum number of statistics that can be held in memory */
    char *format;             /**< Format for printf */
    char *label;              /**< Name to output for this statistic */
    int id_indices[MAX_STAT_ITEM_ID];   /**< Index at which the IDs are stored */
    int num_items;            /**< Number of items currently registered for logging */
    int max_items;            /**< Number of items allocated for logging */
    pthread_rwlock_t lock;    /**< Lock for adding new stat items */
    struct stat_item *items;  /**< The items of this type being logged */
};

#if DUMP_STATS
#define MAX_STATS 262144
#else
/** The maximum number of statistics that can be held in the rrdb */
#define MAX_STATS 8192
#endif

/** If set to 0, will disable all gathering of statistics */
#define GATHER_STATS 1

#define GATHER_MSU_QUEUE_LEN 1 & GATHER_STATS
#define GATHER_MSU_EXEC_TIME 1 & GATHER_STATS
#define GATHER_MSU_IDLE_TIME 1 & GATHER_STATS
#define GATHER_MSU_MEM_ALLOC 1 & GATHER_STATS
#define GATHER_MSU_ITEMS_PROC 1 & GATHER_STATS
#define GATHER_MSU_NUM_STATES 1 & GATHER_STATS
#define GATHER_CUSTOM_STATS 1 & GATHER_STATS
#define GATHER_THREAD_STATS 1 & GATHER_STATS

#ifdef DEDOS_PROFILER
#define GATHER_PROFILING 1
#else
#define GATHER_PROFILING 0
#endif

/** The list of all statistics that can be gathered in the system */
struct stat_type stat_types[] = {
    {MSU_QUEUE_LEN,       GATHER_MSU_QUEUE_LEN,   MAX_STATS, "%02.0f", "MSU_Q_LEN"},
    {MSU_ITEMS_PROCESSED, GATHER_MSU_ITEMS_PROC,  MAX_STATS, "%03.0f", "ITEMS_PROCESSED"},
    {MSU_EXEC_TIME,       GATHER_MSU_EXEC_TIME,   MAX_STATS, "%0.9f",  "MSU_EXEC_TIME"},
    {MSU_IDLE_TIME,       GATHER_MSU_IDLE_TIME,   MAX_STATS, "%0.9f",  "MSU_IDLE_TIME"},
    {MSU_MEM_ALLOC,       GATHER_MSU_MEM_ALLOC,   MAX_STATS, "%09.0f", "MSU_MEM_ALLOC"},
    {MSU_NUM_STATES,      GATHER_MSU_NUM_STATES,  MAX_STATS, "%09.0f", "MSU_NUM_STATES"},
    {THREAD_UCPUTIME,     GATHER_THREAD_STATS,    MAX_STATS, "%09.9f", "THREAD_USER_TIME"},
    {THREAD_SCPUTIME,     GATHER_THREAD_STATS,    MAX_STATS, "%09.9f", "THREAD_KERNEL_TIME"},
    {THREAD_MAXRSS,       GATHER_THREAD_STATS,    MAX_STATS, "%09.0f", "THREAD_MAX_RSS"},
    {THREAD_MINFLT,       GATHER_THREAD_STATS,    MAX_STATS, "%09.0f", "THREAD_MINOR_PG_FAULTS"},
    {THREAD_MAJFLT,       GATHER_THREAD_STATS,    MAX_STATS, "%09.0f", "THREAD_MAJOR_PG_FAULTS"},
    {THREAD_VCSW,         GATHER_THREAD_STATS,    MAX_STATS, "%09.0f", "THREAD_VOL_CTX_SW"},
    {THREAD_IVCSW,        GATHER_THREAD_STATS,    MAX_STATS, "%09.0f", "THREAD_INVOL_CTX_SW"},
    {MSU_STAT1,           GATHER_CUSTOM_STATS,    MAX_STATS, "%03.0f", "MSU_STAT1"},
    {MSU_STAT2,           GATHER_CUSTOM_STATS,    MAX_STATS, "%03.0f", "MSU_STAT2"},
    {MSU_STAT3,           GATHER_CUSTOM_STATS,    MAX_STATS, "%03.0f", "MSU_STAT3"},
    {PROF_ENQUEUE,        GATHER_PROFILING,       MAX_STATS, "%0.0f", "ENQUEUE"},
    {PROF_DEQUEUE,        GATHER_PROFILING,       MAX_STATS, "%0.0f", "DEQUEUE"},
    {PROF_REMOTE_SEND,    GATHER_PROFILING,       MAX_STATS, "%0.0f", "SEND"},
    {PROF_REMOTE_RECV,    GATHER_PROFILING,       MAX_STATS, "%0.0f", "RECV"},
    {PROF_DEDOS_ENTRY,    GATHER_PROFILING,       MAX_STATS, "%0.0f", "ENTRY"},
    {PROF_DEDOS_EXIT,     GATHER_PROFILING,       MAX_STATS, "%0.0f", "EXIT"},
};

/** The number of types of statistics */
#define N_STAT_TYPES (sizeof(stat_types) / sizeof(*stat_types))

#define CHECK_INITIALIZATION \
    if (!stats_initialized) { \
        log_error("Statistics not initialized!"); \
        return -1; \
    }

/* TODO: Incremental flushes of raw items, followed by eventual conversion
static inline int flush_raw_item_to_log(FILE *file, struct stat_type *type,
                                        int item_idx) {
    struct stat_item *item = &type->items[item_idx];
    int max_stats = type->max_stats;

    fwrite(&type->id, sizeof(type->id), 1, file);
    int label_len = strlen(type->label) + 1;
    fwrite(&label_len, sizeof(label_len), 1, file);
    fwrite(type->label, sizeof(char), label_len, file);
    int fmt_len = strlen(type->format) + 1;
    fwrite(&fmt_len, sizeof(fmt_len), 1, file);
    fwrite(type->format, sizeof(char), fmt_len, file);

    fwrite(&item->id, sizeof(item->id), 1, file);
    fwrite(&item->write_index, sizeof(item->write_index), 1, file);
    fwrite(&item->rolled_over, sizeof(item->rolled_over), 1, file);
    int n_stats = item->rolled_over ? max_stats : item->write_index;
    fwrite(&n_stats, sizeof(n_stats), 1, file);
    fwrite(item->stats, sizeof(*item->stats), n_stats, file);

    return 0;
}

static int convert_raw_item_from_log(FILE *in_file, FILE *out_file) {
    struct stat_type type;
    struct stat_item item;

    fread(&type.id, sizeof(type.id), 1, in_file);
    int label_len;
    fread(&label_len, sizeof(label_len), 1, in_file);
    fread(type.label, sizeof(char), label_len, in_file);

    int fmt_len;
    fread(&fmt_len, sizeof(fmt_len), 1, in_file);
    fread(type.format, sizeof(char), fmt_len, in_file);

    fread(&item.id, sizeof(item.id), 1, in_file);
    fread(&item.write_index, sizeof(item.write_index), 1, in_file);
    fread(&item.rolled_over, sizeof(item.rolled_over), 1, in_file);
    int n_stats;
    fread(&n_stats, sizeof(n_stats), 1, in_file);
    item.stats = malloc(sizeof(*item.stats) * n_stats);
    fread(item.stats, sizeof(*item.stats), n_stats, in_file);

    return write_item_to_log(out_file, &type, &item);
}
*/

/** Writes a single stat item to the log file */
static int write_item_to_log(FILE *out_file, struct stat_type *type, struct stat_item *item) {

    char label[64];
    int ln = snprintf(label, 64, "%s:%02d:", type->label, item->id);
    label[ln] = '\0';
    //size_t label_size = strlen(label);

    int n_stats = item->rolled_over ? type->max_stats : item->write_index;
    for (int i=0; i<n_stats; i++) {
        int rtn;
        if ((rtn = fprintf(out_file, "%s", label)) > 30 || rtn < 0) {
            log_warn("printed out %d characters while outputting value %d", rtn, i);
        }

        //fwrite(label, 1, label_size, out_file);
        if ((rtn = fprintf(out_file, "%010ld.%09ld:",
                (long int)item->stats[i].time.tv_sec,
                (long int)item->stats[i].time.tv_nsec)) > 30 || rtn < 0)  {
            log_warn("Printed out %d characters outputting value %d",rtn,  i);
        }
        if ((rtn = fprintf(out_file, type->format, item->stats[i].value)) > 30 || rtn < 0) {
            log_warn("printed out %d characters while outputting value %d", rtn, i);
        }
        fprintf(out_file, "%s", "\n");
    }
    log(LOG_STATS, "Flushed %d items for type %s (rollover: %d)", n_stats, type->label, item->rolled_over);
    return 0;
}


/**
 * Gets the amount of time that has elapsed since logging started.
 * @param *t the elapsed time is output into this variable
*/
static void get_elapsed_time(struct timespec *t) {
   clock_gettime(CLOCK_ID, t);
   //t->tv_sec -= stats_start_time.tv_sec;
}

/** Locks a stat type for writing */
static inline int wrlock_type(struct stat_type *type) {
    if (pthread_rwlock_wrlock(&type->lock) != 0) {
        log_error("Error locking stat type %s", type->label);
        return -1;
    }
    return 0;
}

/** Unlocks a stat type */
static inline int unlock_type(struct stat_type *type) {
    if (pthread_rwlock_unlock(&type->lock) != 0) {
        log_error("Error unlocking stat type %s", type->label);
        return -1;
    }
    return 0;
}

/** Locks a stat type for reading */
static inline int rlock_type(struct stat_type *type) {
    if (pthread_rwlock_rdlock(&type->lock) != 0) {
        log_error("Error locking stat type %s", type->label);
        return -1;
    }
    return 0;
}

/** Locks an item_stats structure, printing an error message on failure
 * @param *item the item_stats structure to lock
 * @return 0 on success, -1 on error
 */
static inline int lock_item(struct stat_item *item) {
    if ( pthread_mutex_lock(&item->mutex) != 0) {
        log_error("Error locking mutex for an item with ID %u", item->id);
        return -1;
    }
    return 0;
}

/** Unlocks an item_stats structure, printing an error message on failure
 * @param *item the item_stats structure to unlock
 * @return 0 on success, -1 on error
 */
static inline int unlock_item(struct stat_item *item) {
    if ( pthread_mutex_unlock(&item->mutex) != 0) {
        log_error("Error locking mutex for an item with ID %u", item->id);
        return -1;
    }
    return 0;
}

/** Writes all statistics to the provided log file */
static void UNUSED flush_all_stats_to_log(FILE *file) {
    for (int i=0; i<N_STAT_TYPES; i++) {
        if (!stat_types[i].enabled) {
            continue;
        }
        rlock_type(&stat_types[i]);
        for (int j=0; j<stat_types[i].num_items; j++) {
            lock_item(&stat_types[i].items[j]);
            write_item_to_log(file, &stat_types[i], &stat_types[i].items[j]);
            unlock_item(&stat_types[i].items[j]);
        }
        unlock_type(&stat_types[i]);
    }
}

/** Frees the memory associated with an individual stat item */
static void destroy_stat_item(struct stat_item *item) {
    free(item->stats);
    pthread_mutex_destroy(&item->mutex);
}

/** Gets the stat item associated with the given item_id in the given type */
static struct stat_item *get_item_stat(struct stat_type *type, unsigned int item_id) {
    if (item_id > MAX_STAT_ITEM_ID) {
        log_error("Item ID %u too high. Max: %d", item_id, MAX_STAT_ITEM_ID);
        return NULL;
    }
    int id_index = type->id_indices[item_id];
    if (id_index == -1) {
        log_error("Item ID %u not initialized for stat %s",
                  item_id, type->label);
        return NULL;
    }
    return &type->items[id_index];
}

static inline long double current_double_time() {
    struct timespec t;
    clock_gettime(DURATION_CLOCK_ID, &t);
    return (long double)t.tv_sec + (long double)t.tv_nsec / 1e9;
}

int record_start_time(enum stat_id stat_id, unsigned int item_id) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    if (rlock_type(type)) {
        return -1;
    }
    struct stat_item *item = get_item_stat(type, item_id);
    unlock_type(type);
    if (item == NULL) {
        return -1;
    }

    if (lock_item(item)) {
        return -1;
    }
    get_elapsed_time(&item->stats[item->write_index].time);
    item->stats[item->write_index].value = current_double_time();
    unlock_item(item);

    return 0;
}

#if DUMP_STATS
#define WARN_ROLLOVER(label, item_id) \
        log_warn("Stats for type %s.%u rolling over. Log file will be missing data", \
                 label, item_id);
#else
#define WARN_ROLLOVER(label, item_id)
#endif


int record_end_time(enum stat_id stat_id, unsigned int item_id) {
    CHECK_INITIALIZATION;

    long double new_time = current_double_time();

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    if (rlock_type(type)) {
        return -1;
    }
    struct stat_item *item = get_item_stat(type, item_id);
    unlock_type(type);
    if (item == NULL) {
        return -1;
    }

    if (lock_item(item)) {
        return -1;
    }

    item->stats[item->write_index].value = new_time - item->stats[item->write_index].value;
    item->write_index++;

    if (item->write_index == type->max_stats) {
        WARN_ROLLOVER(type->label, item_id);
        item->write_index = 0;
        item->rolled_over = true;
    }

    unlock_item(item);
    return 0;
}
int increment_stat(enum stat_id stat_id, unsigned int item_id, double value) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    if (rlock_type(type)) {
        return -1;
    }
    struct stat_item *item = get_item_stat(type, item_id);
    unlock_type(type);
    if (item == NULL) {
        return -1;
    }

    if (lock_item(item)) {
        return -1;
    }

    get_elapsed_time(&item->stats[item->write_index].time);
    int last_index = item->write_index - 1;
    if ( last_index < 0 ) {
        last_index = type->max_stats - 1;
    }
    item->stats[item->write_index].value = item->stats[last_index].value + value;
    item->write_index++;
    if (item->write_index == type->max_stats) {
        WARN_ROLLOVER(type->label, item_id);
        item->write_index = 0;
        item->rolled_over = true;
    }

    unlock_item(item);
    return 0;
}

int record_stat(enum stat_id stat_id, unsigned int item_id, double stat, bool relog) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    if (rlock_type(type)) {
        return -1;
    }
    struct stat_item *item = get_item_stat(type, item_id);
    unlock_type(type);
    if (item == NULL) {
        return -1;
    }

    if (lock_item(item)) {
        return -1;
    }

    int last_index = item->write_index - 1;
    bool do_log = true;
    if (last_index >= 0) {
        do_log = item->stats[last_index].value != stat;
    }

    if (relog || do_log) {
        get_elapsed_time(&item->stats[item->write_index].time);
        item->stats[item->write_index].value = stat;
        item->write_index++;
        if (item->write_index == type->max_stats) {
            WARN_ROLLOVER(type->label, item_id);
            item->write_index = 0;
            item->rolled_over = true;
        }
    }
    unlock_item(item);
    return 0;
}

double get_last_stat(enum stat_id stat_id, unsigned int item_id) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    if (rlock_type(type)) {
        return -1;
    }
    struct stat_item *item = get_item_stat(type, item_id);
    unlock_type(type);
    if (item == NULL) {
        return -1;
    }
    if (lock_item(item)) {
        return -1;
    }

    int last_index = item->write_index - 1;
    double last_stat = 0;
    if (last_index >= 0) {
        last_stat = item->stats[last_index].value;
    }

    unlock_item(item);

    return last_stat;
}

/** Returns 1 if t1 > t2, -1 if t2 > t1, 0 otherwise */
static inline int time_cmp(struct timespec *t1, struct timespec *t2) {
    return t1->tv_sec > t2->tv_sec ? 1 :
            ( t1->tv_sec < t2->tv_sec ? -1 :
             ( t1->tv_nsec > t2->tv_nsec ? 1 :
               ( t1->tv_nsec < t2->tv_nsec ? -1 : 0 ) ) );
}

/** Finds the index at which the given time occurred in the stats */
static int find_time_index(struct timed_stat *stats, struct timespec *time,
                    int start_index, int stat_size) {
    if (start_index < 0) {
        start_index = stat_size - 1;
    }
    if (time_cmp(&stats[start_index].time, time) <= 0) {
        return start_index;
    }
    int search_index = start_index - 1;
    do {
        if (search_index < 0) {
            search_index = stat_size - 1;
        }
        if (time_cmp(&stats[search_index].time, time) <= 0) {
            return search_index;
        }
        search_index--;
    } while (search_index != start_index - 1);
    return -1;
}

/** Takes a sample of the provided stat item and stores it in `sample` */
static int sample_stat_item(struct stat_item *item, int stat_size,
                     struct timespec *sample_end, struct timespec *interval, int sample_size,
                     struct timed_stat *sample) {
    struct timespec sample_time = *sample_end;
    if (lock_item(item)) {
        return -1;
    }
    int sample_index = item->write_index - 1;
    if (sample_index < 0) {
        sample_index = stat_size + sample_index;
    }
    int max_index = item->rolled_over ?  stat_size : item->write_index;
    int i;
    for (i=0; i<sample_size; i++) {
        if (max_index > 1) {
            sample_index = find_time_index(item->stats, &sample_time, sample_index, max_index);
        } else {
            sample_index = 0;
        }
        if (sample_index < 0) {
            log(LOG_STATS, "Couldn't find time index for item %d", item->id);
            unlock_item(item);
            return i;
        }
        sample[sample_size - i - 1].value = item->stats[sample_index].value;
        sample[sample_size - i - 1].time = sample_time;
        sample_time.tv_nsec -= interval->tv_nsec;
        if (sample_time.tv_nsec < 0) {
            sample_time.tv_sec -= 1;
            sample_time.tv_nsec = 1e9 + sample_time.tv_nsec;
        }
        sample_time.tv_sec -= interval->tv_sec;
    }
    unlock_item(item);
    return i;
}

/** Takes a sample of the provided stat type (stat_id) and places it in `sample` */
static int sample_stat(enum stat_id stat_id, struct timespec *end, struct timespec *interval,
                int sample_size, struct stat_sample *sample, int n_samples) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        log_error("Attempted to sample disabled statistic %d", stat_id);
        return -1;
    }


    if (n_samples < type->num_items) {
        log_error("Not enough samples (%d) to fit all stat items (%d)", n_samples, type->num_items);
        return -1;
    }

    int item_id = -1;
    for (int i=0; i<type->num_items; i++) {
        sample[i].hdr.stat_id = stat_id;
        // Find the next item index
        for (item_id += 1;
                item_id < MAX_STAT_ITEM_ID && type->id_indices[item_id] == -1;
                item_id++) {}
        log(LOG_TEST, "Found :%d", item_id);
        if (item_id == MAX_STAT_ITEM_ID) {
            log_error("Cannot find item ID %d", i);
            return -1;
        }
        int idx = type->id_indices[item_id];
        sample[i].hdr.item_id = type->items[idx].id;
        sample[i].hdr.n_stats = sample_size;
        int rtn = sample_stat_item(&type->items[idx], type->max_stats, end, interval, sample_size,
                                    sample[i].stats);

        if ( rtn < 0) {
            log_error("Error sampling item %d (idx %d)", type->items[i].id, i);
            return -1;
        } else {
            sample[i].hdr.n_stats = rtn;
        }
    }
    return type->num_items;
}

/** Samples `sample_size` most recent stats in `interval` intervals */
static int sample_recent_stats(enum stat_id stat_id, struct timespec *interval,
                               int sample_size, struct stat_sample *sample, int n_samples) {
    CHECK_INITIALIZATION;
    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    struct timespec now;
    get_elapsed_time(&now);
    return sample_stat(stat_id, &now, interval, sample_size, sample, n_samples);
}

/** A statically allocated stat_sample structure which is pre-allocated to store samples
 * from each stat type */
static struct stat_sample *stat_samples[N_REPORTED_STAT_TYPES];

/** The interval at which stats should be sampled */
static struct timespec stat_sample_interval = {
    .tv_sec = (STAT_SAMPLE_PERIOD_MS / 1000) / STAT_SAMPLE_SIZE,
    .tv_nsec = (int)(STAT_SAMPLE_PERIOD_MS * 1e6 / STAT_SAMPLE_SIZE) % (int)1e9
};

struct stat_sample *get_stat_samples(enum stat_id stat_id, int *n_samples_out) {
    if (!stats_initialized) {
        log_error("Stats not initialized");
        return NULL;
    }

    *n_samples_out = 0;
    int i;
    for (i=0; i < N_REPORTED_STAT_TYPES; i++) {
        if (reported_stat_types[i].id == stat_id) {
            break;
        }
    }
    if (i == N_REPORTED_STAT_TYPES) {
        log_error("%d is not a reported stat type", stat_id);
        return NULL;
    }
    if (stat_samples[i] == NULL) {
        // No stat samples registered!"
        return NULL;
    }
    struct stat_type *type = &stat_types[stat_id];
    if (rlock_type(type)) {
        return NULL;
    }
    int rtn = sample_recent_stats(stat_id, &stat_sample_interval, STAT_SAMPLE_SIZE,
                                  stat_samples[i], type->num_items);
    unlock_type(type);
    if (rtn < 0) {
        log_error("Error sampling recent stats");
        return NULL;
    }
    *n_samples_out = rtn;
    return stat_samples[i];
}

/** Reallocates the existing stats structure to store the provided number of samples */
static void realloc_stat_samples(enum stat_id stat_id, int n_samples) {
    for (int i=0; i < N_REPORTED_STAT_TYPES; i++) {
        if (reported_stat_types[i].id == stat_id) {
            free_stat_samples(stat_samples[i], n_samples - 1);
            stat_samples[i] = init_stat_samples(STAT_SAMPLE_SIZE, n_samples);
            log(LOG_STAT_REALLOC, "Reallocated %d to %d samples", stat_id, n_samples);
            return;
        }
    }
}

int remove_stat_item(enum stat_id stat_id, unsigned int item_id) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    if (wrlock_type(type) != 0) {
        return -1;
    }
    if (type->id_indices[item_id] == -1) {
        log_error("Item ID %u not assigned an index", item_id);
        return -1;
    }

    int index = type->id_indices[item_id];
    struct stat_item *item = &type->items[index];

    item->id = -1;
    item->write_index = 0;
    item->rolled_over = false;
    free(item->stats);

    type->id_indices[item_id] = -1;
    type->num_items--;
    unlock_type(type);
    return 0;
}


/**
 * Initializes a stat item so that statistics can be logged to it.
 * (e.g. initializing MSU_QUEUE_LEN for item N, corresponding to msu # N)
 * @param stat_id ID of the statistic type to be logged
 * @param item_id ID of the item to be logged
 */
int init_stat_item(enum stat_id stat_id, unsigned int item_id) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        log(LOG_STATS, "Skipping initialization of type %s item %d",
                   type->label, item_id);
        return 0;
    }
    if (wrlock_type(type) != 0) {
        return -1;
    }
    if (type->id_indices[item_id] != -1) {
        log_error("Item ID %u already assigned index %d for stat %s",
                item_id, type->id_indices[item_id], type->label);
        return -1;
    }
    int index;
    for (index=0; index<type->max_items; index++) {
        if (type->items[index].id == -1) {
            break;
        }
    }
    if (index == type->max_items) {
        type->max_items++;
        type->items = realloc(type->items, sizeof(*type->items) * (type->max_items));
        realloc_stat_samples(stat_id, type->max_items);

        if ( type->items == NULL ) {
            log_error("Error reallocating stat item");
            return -1;
        }
    }
    log(LOG_STATS, "Item %u has index %d", item_id, index);
    struct stat_item *item = &type->items[index];

    if (pthread_mutex_init(&item->mutex,NULL)) {
        log_error("Error initializing item %u lock", item_id);
        return -1;
    }

    item->id = item_id;
    item->write_index = 0;
    item->rolled_over = false;
    item->stats = calloc(type->max_stats, sizeof(*item->stats));

    if (item->stats == NULL) {
        log_error("Error calloc'ing item statistics");
        return -1;
    }

    type->id_indices[item_id] = index;

    type->num_items++;

    if (unlock_type(type) != 0) {
        return -1;
    }

    log(LOG_STAT_INITS, "Initialized stat item %s.%d (idx: %d)",
            stat_types[stat_id].label, item_id, index);
    return 0;
}

int init_statistics() {
    if ( stats_initialized ) {
        log_error("Statistics have already been initialized");
        return -1;
    }

    clock_gettime(CLOCK_ID, &stats_start_time);

    for (int i = 0; i < N_STAT_TYPES; i++) {
        struct stat_type *type = &stat_types[i];
        if ( type->id != i ) {
            log_error("Stat type %s at incorrect index (%d, not %d)! Disabling!!",
                       type->label, i, type->id);
            type->enabled = 0;
        }

        if (type->enabled) {
            if (pthread_rwlock_init(&type->lock, NULL) != 0) {
                log_warn("Error initializing lock for stat %s. Disabling!", type->label);
                type->enabled = 0;
            }
            type->num_items = 0;
            type->items = NULL;
            for (int j = 0; j < MAX_STAT_ITEM_ID; j++) {
                type->id_indices[j] = -1;
            }
            log(LOG_STATS, "Initialized stat type %s (%d ids)", type->label, MAX_STAT_ITEM_ID);
        }
    }

#if DUMP_STATS
    log_info("Statistics initialized for eventual dump to log file");
#else
    log_info("Statistics initialized. Will **NOT** be dumped to log file");
#endif
    stats_initialized = true;
    return 0;
}

void finalize_statistics(char *statlog) {
    if (statlog != NULL) {
#if DUMP_STATS
        log_info("Outputting stats to log. Do not quit.");
        FILE *file = fopen(statlog, "w");
        flush_all_stats_to_log(file);
        fclose(file);
        log_info("Finished outputting stats to %s", statlog);
#else
        log_info("Skipping dump to stat log. Set DUMP_STATS=1 to enable");
#endif
    }

    for (int i = 0; i < N_STAT_TYPES; i++) {
        struct stat_type *type = &stat_types[i];
        if (type->enabled) {
            wrlock_type(type);
            for (int j = 0; j < type->num_items; j++) {
                log(LOG_STAT_INITS, "Destroying item %s.idx=%d",
                           type->label, j);
                destroy_stat_item(&type->items[j]);
            }
            free(type->items);
            unlock_type(type);
            pthread_rwlock_destroy(&type->lock);
        }
    }
}
