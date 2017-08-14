#include <stdbool.h>

#ifndef LOG_STATS
#define LOG_STATS 0
#endif

#define CLOCK_ID CLOCK_MONOTONIC


static struct timespec stats_start_time;
static bool stats_initialized = false

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
    pthread_rwlock_t rwlock   /**< Lock for changing stat item */
    // TODO: Do I need `sample`? 
    //int last_sample_index;    /**< Index at which the stat was sampled last */
    //struct timed_stat sample[MAX_STAT_SAMPLES]; /**< Filled with stat samples on call to
    //                                                 sample_stats() */
};

struct stat_type {
    enum stat_id id;          /**< Stat ID as defined in stats.h */
    int enabled;              /**< If 1, logging for this item is enabled */
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


struct stat_type stat_types[] = {
    {MSU_QUEUE_LEN,       GATHER_MSU_QUEUE_LEN,   MAX_STATS, "%02.0f", "MSU_Q_LEN"},
    {MSU_ITEMS_PROCESSED, GATHER_MSU_ITEMS_PROC,  MAX_STATS, "%03.0f", "ITEMS_PROCESSED"},
    {MSU_EXEC_TIME,       GATHER_MSU_EXEC_TIME,   MAX_STATS, "%0.9f",  "MSU_EXEC_TIME"},
    {MSU_IDLE_TIME,       GATHER_MSU_IDLE_TIME,   MAX_STATS, "%0.9f",  "MSU_IDLE_TIME"},
    {MSU_MEM_ALLOC,       GATHER_MSU_MEM_ALLOC,   MAX_STATS, "%09.0f", "MSU_MEM_ALLOC"},
    {MSU_NUM_STATES,      GATHER_MSU_NUM_STATES,  MAX_STATS, "%09.0f", "MSU_NUM_STATES"},
    {THREAD_CTX_SWITCHES,        GATHER_CTX_SWITCHES,    MAX_STATS, "%03.0f", "CTX_SWITCHES"}
} 

#define N_STAT_TYPES (sizeof(stat_types) / sizeof(struct stat_type))

/** Locks a stat type for writing */
static int wrlock_type(struct stat_type *type) {
    if (pthread_rwlock_wrlock(&type->lock) != 0) {
        log_error("Error locking stat type %s", type->label);
        return -1;
    }
    return 0;
}

/** Unlocks a stat type */
static int unlock_type(struct stat_type *type) {
    if (pthread_rwlock_unlock(&type->lock) != 0) {
        log_error("Error unlocking stat type %s", type->label);
        return -1;
    }
    return 0;
}

/** Frees the memory associated with an individual stat item */
static void destroy_stat_item(struct stat_item *item) {
    free(item->stats);
    pthread_rwlock_destroy(&item->rwlock);
}

/** 
 * Initializes a stat item so that statistics can be logged to it.
 * (e.g. initializing MSU_QUEUE_LEN for item N, corresponding to msu # N)
 * @param stat_id ID of the statistic type to be logged
 * @param item_id ID of the item to be logged
 */
int init_stat_item(enum stat_id stat_id, unsigned int item_id) {
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
        log_error("Item ID %u already assigned index %d", item_id, id_indices[item_id]);
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

    if (pthread_rwlock_init(&item->rwlock)) {
        log_error("Error initializing item %u lock", item_id);
        item->num_items--;
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
            if (pthread_rwlock_init(&type->rwlock, NULL) != 0) {
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
        flush_all_stats_to_log(statlog);
        log_info("Finished outputting stats to log");
#else
        log_info("Skipping dump to stat log. Set DUMP_STATS=1 to enable");
#endif
    }

    for (int i = 0; i < N_STAT_TYPES; i++) {
        struct stat_type *type = &stat_types[i];
        if (type->enabled) {
            pthread_rwlock_wrlock(&type->rwlock);
            for (int j = 0; j < type->num_items; i++) {
                destroy_stat_item(&type->items[i]);
            }
            free(type->items);
            pthread_rwlock_unlock(&type->rwlock);
            pthread_rwlock_destroy(&type->rwlock);
        }
    }
}
