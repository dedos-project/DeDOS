#include "rt_stats.h"
#include "stats.h"
#include "logging.h"

#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#ifndef LOG_STATS
#define LOG_STATS 0
#endif

#define CLOCK_ID CLOCK_MONOTONIC

static struct timespec stats_start_time;
static bool stats_initialized = false;

#define MAX_ID 64

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
    // TODO: Do I need `sample`?
    //int last_sample_index;    /**< Index at which the stat was sampled last */
    //struct timed_stat sample[MAX_STAT_SAMPLES]; /**< Filled with stat samples on call to
    //                                                 sample_stats() */
};

struct stat_type {
    enum stat_id id;          /**< Stat ID as defined in stats.h */
    bool enabled;             /**< If true, logging for this item is enabled */
    int max_stats;            /**< Maximum number of statistics that can be held in memory */
    char *format;             /**< Format for printf */
    char *label;              /**< Name to output for this statistic */
    int id_indices[MAX_ID];   /**< Index at which the IDs are stored */
    int num_items;            /**< Number of items currently registered for logging */
    pthread_rwlock_t lock;    /**< Lock for adding new stat items */
    struct stat_item *items;  /**< The items of this type being logged */
};

#if DUMP_STATS
#define MAX_STATS 262144
#else
#define MAX_STATS 8192
#endif

#define MAX_MSU 32
#define MAX_THREADS 12

#define GATHER_CTX_SWITCHES 1
#define GATHER_MSU_QUEUE_LEN 1
#define GATHER_MSU_EXEC_TIME 1
#define GATHER_MSU_IDLE_TIME 1
#define GATHER_MSU_MEM_ALLOC 1
#define GATHER_MSU_ITEMS_PROC 1
#define GATHER_MSU_NUM_STATES 1
#define GATHER_CUSTOM_STATS 1


struct stat_type stat_types[] = {
    {MSU_QUEUE_LEN,       GATHER_MSU_QUEUE_LEN,   MAX_STATS, "%02.0f", "MSU_Q_LEN"},
    {MSU_ITEMS_PROCESSED, GATHER_MSU_ITEMS_PROC,  MAX_STATS, "%03.0f", "ITEMS_PROCESSED"},
    {MSU_EXEC_TIME,       GATHER_MSU_EXEC_TIME,   MAX_STATS, "%0.9f",  "MSU_EXEC_TIME"},
    {MSU_IDLE_TIME,       GATHER_MSU_IDLE_TIME,   MAX_STATS, "%0.9f",  "MSU_IDLE_TIME"},
    {MSU_MEM_ALLOC,       GATHER_MSU_MEM_ALLOC,   MAX_STATS, "%09.0f", "MSU_MEM_ALLOC"},
    {MSU_NUM_STATES,      GATHER_MSU_NUM_STATES,  MAX_STATS, "%09.0f", "MSU_NUM_STATES"},
    {THREAD_CTX_SWITCHES, GATHER_CTX_SWITCHES,    MAX_STATS, "%03.0f", "CTX_SWITCHES"},
    {MSU_STAT1,           GATHER_CUSTOM_STATS,    MAX_STATS, "%03.0f", "MSU_STAT1"},
    {MSU_STAT2,           GATHER_CUSTOM_STATS,    MAX_STATS, "%03.0f", "MSU_STAT2"},
    {MSU_STAT3,           GATHER_CUSTOM_STATS,    MAX_STATS, "%03.0f", "MSU_STAT3"}
};

#define N_STAT_TYPES (sizeof(stat_types) / sizeof(struct stat_type))

#define CHECK_INITIALIZATION \
    if (!stats_initialized) { \
        log_error("Statistics not initialized!"); \
        return -1; \
    }

/**
 * Gets the amount of time that has elapsed since logging started.
 * @param *t the elapsed time is output into this variable
*/
void get_elapsed_time(struct timespec *t) {
   clock_gettime(CLOCK_ID, t);
   t->tv_sec -= stats_start_time.tv_sec;
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

/** Frees the memory associated with an individual stat item */
static void destroy_stat_item(struct stat_item *item) {
    free(item->stats);
    pthread_mutex_destroy(&item->mutex);
}

static struct stat_item *get_item_stat(struct stat_type *type, unsigned int item_id) {
    if (item_id > MAX_ID) {
        log_error("Item ID %u too high. Max: %d", item_id, MAX_ID);
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

/**
 * Starts a measurement of elapsed time.
 * Not added to the log until a call to record_end_time
 * @param stat_id ID for stat type being logged
 * @param item_id ID for the item to which the stat refers
 * @return 0 on success, -1 on error
 */
int record_start_time(enum stat_id stat_id, unsigned int item_id) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    rlock_type(type);
    struct stat_item *item = get_item_stat(type, item_id);
    unlock_type(type);
    if (item == NULL) {
        return -1;
    }

    lock_item(item);
    get_elapsed_time(&item->stats[item->write_index].time);
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


/**
 * Records the elapsed time since the previous call to record_start_time
 * @param stat_id ID for stat being logged
 * @param item_id ID for item within the statistic
 */
int record_end_time(enum stat_id stat_id, unsigned int item_id) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    rlock_type(type);
    struct stat_item *item = get_item_stat(type, item_id);
    unlock_type(type);
    if (item == NULL) {
        return -1;
    }

    struct timespec new_time;
    get_elapsed_time(&new_time);

    lock_item(item);
    time_t timediff_s = new_time.tv_sec - item->stats[item->write_index].time.tv_sec;
    long timediff_ns = new_time.tv_nsec - item->stats[item->write_index].time.tv_nsec;
    item->stats[item->write_index].value  =
            (double)timediff_s + ((double)timediff_ns/(1e9));
    item->write_index++;

    if (item->write_index == type->max_stats) {
        WARN_ROLLOVER(type->label, item_id);
        item->write_index = 0;
        item->rolled_over = true;
    }

    unlock_item(item);
    return 0;
}

/**
 * Increments the given statistic by the provided value
 * @param stat_id ID for the stat being logged
 * @param item_id ID for the item to which the stat refers (must be registered!)
 * @param value The amount to add to the given stat
 * @return 0 on success, -1 on error
 */
int increment_stat(enum stat_id stat_id, unsigned int item_id, double value) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    rlock_type(type);
    struct stat_item *item = get_item_stat(type, item_id);
    unlock_type(type);
    if (item == NULL) {
        return -1;
    }

    lock_item(item);

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

/**
 * Records a statistic in the statlog
 * @param stat_id ID for stat being logged
 * @param item_id ID for item to which stat refers (must be registered!)
 * @param stat Statistic to record
 * @param relog Whether to log statistic if it matches the previously logged stat
 * @return 0 on success, -1 on error
 */
int record_stat(enum stat_id stat_id, unsigned int item_id, double stat, bool relog) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    rlock_type(type);
    struct stat_item *item = get_item_stat(type, item_id);
    unlock_type(type);
    if (item == NULL) {
        return -1;
    }

    lock_item(item);

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

static inline int time_cmp(struct timespec *t1, struct timespec *t2) {
    return t1->tv_sec > t2->tv_sec ? 1 :
            ( t1->tv_sec < t2->tv_sec ? -1 :
             ( t1->tv_nsec > t2->tv_nsec ? 1 :
               ( t1->tv_nsec < t2->tv_nsec ? -1 : 0 ) ) );
}

static int find_time_index(struct timed_stat *stats, struct timespec *time,
                    int start_index, int stat_size) {
    if (time_cmp(&stats[start_index].time, time) <= 0) {
        return start_index;
    }
    int search_index = start_index - 1;
    while (search_index != start_index) {
        if (time_cmp(&stats[search_index].time, time) <= 0) {
            return search_index;
        }
        search_index--;
        if (search_index < 0) {
            search_index = stat_size - 1;
        }
    }
    return -1;
}

static int sample_stat_item(struct stat_item *item, int stat_size,
                     struct timespec *sample_end, struct timespec *interval, int sample_size,
                     struct timed_stat *sample) {
    struct timespec sample_time = *sample_end;
    lock_item(item);
    int sample_index = item->write_index - 1;
    int i;
    for (i=0; i<sample_size; i++) {
        sample_index = find_time_index(item->stats, &sample_time, sample_index, stat_size);
        if (sample_index < 0) {
            log_error("Couldn't find time index for item %d", item->id);
            unlock_item(item);
            return -1;
        }
        sample[i] = item->stats[sample_index];
        sample_time.tv_nsec -= interval->tv_nsec;
        if (sample_time.tv_nsec < 0) {
            sample_time.tv_sec -= 1;
            sample_time.tv_nsec = 1e9 + sample_time.tv_nsec;
        }
        sample_time.tv_sec -= interval->tv_sec;
    }
    unlock_item(item);
    return 0;
}

static int sample_stat(enum stat_id stat_id, struct timespec *end, struct timespec *interval,
                int sample_size, struct stat_sample *sample, int n_samples) {
    CHECK_INITIALIZATION;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        log_error("Attempted to sample disabled statistic %d", stat_id);
        return -1;
    }

    rlock_type(type);
    if (n_samples < type->num_items) {
        log_error("Not enough samples (%d) to fit all stat items (%d)", n_samples, type->num_items);
        unlock_type(type);
        return -1;
    }

    for (int i=0; i<type->num_items; i++) {
        sample[i].hdr.stat_id = stat_id;
        sample[i].hdr.item_id = type->items[i].id;
        sample[i].hdr.n_stats = sample_size;
        int rtn = sample_stat_item(&type->items[i], sample_size, end, interval, sample_size,
                                    sample[i].stats);

        if ( rtn < 0) {
            log_error("Error sampling item %d (idx %d)", type->items[i].id, i);
            unlock_type(type);
            return -1;
        }
    }
    unlock_type(type);
    return type->num_items;
}

int n_stat_items(enum stat_id stat_id) {
    CHECK_INITIALIZATION;
    return stat_types[stat_id].num_items;
}


int sample_recent_stats(enum stat_id stat_id, struct timespec *interval,
                        int sample_size, struct stat_sample *sample, int n_samples) {
    struct timespec now;
    get_elapsed_time(&now);
    return sample_stat(stat_id, &now, interval, sample_size, sample, n_samples);
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
        log_custom(LOG_STATS, "Skipping initialization of type %s item %d",
                   type->label, item_id);
        return 0;
    }
    if (wrlock_type(type) != 0) {
        return -1;
    }
    if (type->id_indices[item_id] != -1) {
        log_error("Item ID %u already assigned index %d",
                item_id, type->id_indices[item_id]);
        return -1;
    }
    int index = type->num_items;
    type->num_items++;
    type->items = realloc(type->items, sizeof(*type->items) * type->num_items);
    if ( type->items == NULL ) {
        log_error("Error reallocating stat item");
        return -1;
    }
    struct stat_item *item = &type->items[index];

    if (pthread_mutex_init(&item->mutex,NULL)) {
        log_error("Error initializing item %u lock", item_id);
        type->num_items--;
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

    if (unlock_type(type) != 0) {
        return -1;
    }
    return 0;
}

/**
 * Initializes the statistics data structures
 */
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
            for (int j = 0; j < MAX_ID; j++) {
                type->id_indices[j] = -1;
            }
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

/**
 * Destroys the statistics structure and dumps the statistics to disk if applicable
 * @param statlog Log file to dump statistics to or NULL if N/A
 */
void finalize_statistics(char *statlog) {
    if (statlog != NULL) {
#if DUMP_STATS
        log_info("Outputting stats to log. Do not quit.");
        // TODO: flush to log
        //flush_all_stats_to_log(statlog);
        log_info("Finished outputting stats to log");
#else
        log_info("Skipping dump to stat log. Set DUMP_STATS=1 to enable");
#endif
    }

    for (int i = 0; i < N_STAT_TYPES; i++) {
        struct stat_type *type = &stat_types[i];
        if (type->enabled) {
            pthread_rwlock_wrlock(&type->lock);
            for (int j = 0; j < type->num_items; i++) {
                destroy_stat_item(&type->items[i]);
            }
            free(type->items);
            pthread_rwlock_unlock(&type->lock);
            pthread_rwlock_destroy(&type->lock);
        }
    }
}
