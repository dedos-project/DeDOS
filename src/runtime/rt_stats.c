#include "rt_stats.h"
#include "filedes.h"
#include "dfg.h"
#include "runtime_dfg.h"
#include "logging.h"

#include <stdbool.h>

/** The clock used to mark the time at which events take place */
#define CLOCK_ID CLOCK_REALTIME_COARSE
#define DURATION_CLOCK_ID CLOCK_MONOTONIC

#define INCREMENT_START_TO_END(start, end) \
    do { \
        end.tv_nsec = start.tv_nsec + STAT_SAMPLE_PERIOD_MS * 1e6; \
        end.tv_sec = start.tv_sec + (end.tv_nsec / 1e9); \
        end.tv_nsec %= (long)1e9; \
    } while (0)

#define MAX_STATS_PER_SECOND 20000

//#define STAT_LIST_SIZE MAX_STATS_PER_SECOND * STAT_SAMPLE_PERIOD_MS / 1000
#define STAT_LIST_SIZE 5000
struct stat_list
{
    // I don't know if the mutex gains anything due to the
    // order of the mutexes...
    pthread_mutex_t record_lock;
    bool recorded;

    struct timespec start_time;
    pthread_mutex_t writer_mutex;
    unsigned int write_index; /**< The next index that will be written to */

    /** Timestamp and data for each gathered statistic */
    struct timed_stat stats[STAT_LIST_SIZE];
};

#define TIME_CMP(start, end) \
    ((start).tv_sec > (end).tv_sec) ? 1 : \
    ((start).tv_sec < (end).tv_sec) ? -1 : \
    ((start).tv_nsec > (end).tv_nsec) ? 1 : \
    ((start).tv_nsec < (end).tv_nsec) ? -1 : 0

#define ITEM_GROUP_SIZE 3

struct stat_item_group
{
    enum stat_id stat;
    struct stat_referent referent;

    struct timespec last_start_time;

    pthread_mutex_t last_stat_lock;
    struct timed_stat last_stat;

    pthread_mutex_t write_index_lock;
    unsigned int write_index;

    struct stat_list stat_list[ITEM_GROUP_SIZE];
};

#define RT_ITEM_ID 1

#define MAX_RT_ITEM_ID (RT_ITEM_ID + 1)

#define THREAD_ITEM_ID(th) (MAX_RT_ITEM_ID + (th + 1))
#define THREAD_FROM_ITEM_ID(id) ((id) - MAX_RT_ITEM_ID - 1)

#define MAX_THREAD_ITEM_ID THREAD_ITEM_ID(MAX_THREADS + 1)

#define MSU_ITEM_ID(ms) (MAX_THREAD_ITEM_ID + ms)
#define MSU_FROM_ITEM_ID(id) (id - MAX_THREAD_ITEM_ID)

#define MAX_MSU_ITEM_ID MSU_ITEM_ID(MAX_MSU)

#define MAX_ITEM_ID MAX_MSU_ITEM_ID

/** Assumes one of r, t, m > 0 */
#define ITEM_ID(r, t, m) \
    r > 0 ? RT_ITEM_ID : \
    t > 0 ? THREAD_ITEM_ID(t) : m

#define ITEM_ID_REFERENT_TYPE(id) \
    id > MAX_MSU_ITEM_ID ? -1 : \
    id > MAX_THREAD_ITEM_ID ? MSU_STAT : \
    id > MAX_RT_ITEM_ID ? THREAD_STAT : \
    RT_STAT

static unsigned int id_from_item_id(unsigned int id) {
    switch (ITEM_ID_REFERENT_TYPE(id)) {
        case MSU_STAT: return MSU_FROM_ITEM_ID(id);
        case THREAD_STAT: return THREAD_FROM_ITEM_ID(id);
        case RT_STAT: return local_runtime_id();
    }
    return -1;
}

struct stat_type {
    struct stat_label stat;
    bool enabled;

    pthread_mutex_t id_mutex;

    int (*limit_fn)(double*);

    unsigned int max_used_id_index;
    unsigned int used_ids[MAX_ITEM_ID];
    struct stat_item_group *stat_items[MAX_ITEM_ID];
};

/** If set to 0, will disable all gathering of statistics */
#define GATHER_STATS 1

#if defined(DEDOS_PROFILER) & GATHER_STATS
#define GATHER_PROF 1
#else
#define GATHER_PROF 0
#endif


#define DEF_STAT(stat) \
    {_STNAM(stat), GATHER_STATS, PTHREAD_MUTEX_INITIALIZER}

#define DEF_LIMITED_STAT(stat, lim_fn) \
    {_STNAM(stat), GATHER_STATS, PTHREAD_MUTEX_INITIALIZER, lim_fn}

#define DEF_PROF_STAT(stat) \
    {_STNAM(stat), GATHER_PROF, PTHREAD_MUTEX_INITIALIZER}

/**
 * Statistics should be defined in the same order as the enumerator,
 * or else indices would have to be looked up on every call.
 *
 * It is assumed this is followed unless WARN_UNCHECKED_STATS is defined.
 */
static struct stat_type stat_types[] = {
    DEF_STAT(MEM_ALLOC),
    DEF_LIMITED_STAT(FILEDES, proc_fd_limit),
    DEF_STAT(ERROR_CNT),
    DEF_STAT(QUEUE_LEN),
    DEF_STAT(ITEMS_PROCESSED),
    DEF_STAT(EXEC_TIME),
    DEF_STAT(IDLE_TIME),
    DEF_STAT(NUM_STATES),
    DEF_STAT(UCPUTIME),
    DEF_STAT(SCPUTIME),
    DEF_STAT(MAXRSS),
    DEF_STAT(MINFLT),
    DEF_STAT(MAJFLT),
    DEF_STAT(VCSW),
    DEF_STAT(IVCSW),
    DEF_STAT(MSU_STAT1),
    DEF_STAT(MSU_STAT2),
    DEF_STAT(MSU_STAT3),
    DEF_PROF_STAT(PROF_ENQUEUE),
    DEF_PROF_STAT(PROF_DEQUEUE),
    DEF_PROF_STAT(PROF_REMOTE_SEND),
    DEF_PROF_STAT(PROF_REMOTE_RECV),
    DEF_PROF_STAT(PROF_DEDOS_ENTRY),
    DEF_PROF_STAT(PROF_DEDOS_EXIT)
};

#define N_STAT_TYPES (sizeof(stat_types) / sizeof(*stat_types))

/** If this is defined, each access to statistics will be accompanied
 * by a warning if the order of stats has not been checked */
#define WARN_UNCHECKED_STATS

#ifdef WARN_UNCHECKED_STATS
#define ORDER_CHECK \
    if (!stats_checked) { \
        log_warn("Order of statistics is being checked during execution"); \
        check_statistics(); \
    }
#else
#define CHECK_STATS
#endif

/** Whether ::check_statistics has been called */
static bool stats_checked = false;

int check_statistics() {
    int rtn = 0;
    for (int i=0; i < N_STAT_TYPES; i++) {
        struct stat_type *type = &stat_types[i];
        if (type->stat.id != i) {
            log_error("Stat type %s (%d) at incorrect index %d! Disabling!!",
                      type->stat.label, type->stat.id, i);
            type->enabled = 0;
            rtn = 1;
        }
        bool found = false;
        for (int j=0; j < N_REPORTED_STAT_TYPES; j++) {
            if (reported_stat_types[j].id == i) {
                found = true;
                break;
            }
        }
        if (!found) {
            log_warn("Disabling stat type %s because it is not being reported", type->stat.label);
            type->enabled = 0;
        }
    }
    stats_checked = true;
    return rtn;
}

static int lock_type(struct stat_type *type) {
    if (pthread_mutex_lock(&type->id_mutex) != 0) {
        log_error("Failed to lock type %s mutex", type->stat.label);
        return -1;
    }
    return 0;
}

static int unlock_type(struct stat_type *type) {
    if (pthread_mutex_unlock(&type->id_mutex) != 0) {
        log_error("Failed to unlock type %s mutex", type->stat.label);
        return -1;
    }
    return 0;
}

#define CHECK_ID_MAX(id) \
    if (id > MAX_ITEM_ID) { \
        log_error("Item ID %u too high!", id); \
        return NULL; \
    }

static struct stat_item_group *get_item_group(struct stat_type *type,
                                              unsigned int item_id) {
    CHECK_ID_MAX(item_id);
    struct stat_item_group *group = type->stat_items[item_id];
    if (group == NULL) {
        log_error("Cannot get item %u from stat %s",
                  item_id, type->stat.label);
        return NULL;
    }
    return group;
}

static struct stat_list *get_locked_write_stat_list(struct stat_item_group *group,
                                                    unsigned int item_id,
                                                    struct timespec *cur_time) {

    pthread_mutex_lock(&group->write_index_lock);

    struct stat_list *old_list = &group->stat_list[group->write_index];
    struct stat_list *new_list = old_list;

    int locked = pthread_mutex_trylock(&old_list->record_lock);

    if (locked || old_list->recorded) {
        log(LOG_STATS, "Switching to new stat list");
        group->write_index = (group->write_index + 1) % ITEM_GROUP_SIZE;
        new_list = &group->stat_list[group->write_index];

        pthread_mutex_lock(&new_list->writer_mutex);
        new_list->recorded = false;
        new_list->write_index = 0;
        pthread_mutex_unlock(&new_list->writer_mutex);

        new_list->start_time = *cur_time;
        if (locked == 0)
            pthread_mutex_unlock(&old_list->record_lock);
        pthread_mutex_lock(&new_list->record_lock);
    }
    pthread_mutex_unlock(&group->write_index_lock);
    return new_list;
}

static void unlock_stat_list(struct stat_list *list) {
    pthread_mutex_unlock(&list->record_lock);
}

#define timestamp(t) clock_gettime(CLOCK_ID, t);
#define dur_timestamp(t) clock_gettime(DURATION_CLOCK_ID, t);

#define timediff_dbl(t1, t2) (double) (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) * 1e-9


#define DEF_INDIV_FNS(fn, msu_fn_name, thread_fn_name, rt_fn_name) \
    int msu_fn_name(enum stat_id sid, unsigned int mid) { \
        return fn(sid, MSU_ITEM_ID(mid)); \
    } \
    int thread_fn_name(enum stat_id sid, unsigned int tid) { \
        return fn(sid, THREAD_ITEM_ID(tid)); \
    } \
    int rt_fn_name(enum stat_id sid) { \
        return fn(sid, RT_ITEM_ID); \
    }

#define DEF_INDIV_FNS_WARG(fn, msu_fn_name, thread_fn_name, rt_fn_name, argtype) \
    int msu_fn_name(enum stat_id sid, unsigned int mid, argtype _arg) { \
        return fn(sid, MSU_ITEM_ID(mid), _arg); \
    } \
    int thread_fn_name(enum stat_id sid, unsigned int tid, argtype _arg) { \
        return fn(sid, THREAD_ITEM_ID(tid), _arg); \
    } \
    int rt_fn_name(enum stat_id sid, argtype _arg) { \
        return fn(sid, RT_ITEM_ID, _arg); \
    }

static int _record_stat(enum stat_id stat_id, unsigned int item_id, double value, bool lock) {
    log(LOG_STATS, "Recording stat %d.%u", stat_id, item_id);
    struct timespec t;
    timestamp(&t);

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    struct stat_item_group *group = get_item_group(type, item_id);
    if (group == NULL) {
        return -1;
    }

    if (lock)
        pthread_mutex_lock(&group->last_stat_lock);

    struct stat_list *list = get_locked_write_stat_list(group, item_id, &t);

    pthread_mutex_lock(&list->writer_mutex);

    int idx = list->write_index;
    if (idx >= STAT_LIST_SIZE) {
        log_warn("Cannot record statistic %s.%u (%d-%u)! Too many already recorded!",
                 type->stat.label, item_id, ITEM_ID_REFERENT_TYPE(item_id), id_from_item_id(item_id));
        if (lock)
            pthread_mutex_unlock(&group->last_stat_lock);
        return -1;
    }
    list->stats[idx].time = t;
    list->stats[idx].value = value;
    list->write_index = (idx + 1);


    group->last_stat = list->stats[idx];


    pthread_mutex_unlock(&list->writer_mutex);

    unlock_stat_list(list);

    if (lock)
        pthread_mutex_unlock(&group->last_stat_lock);

    log(LOG_RECORD_STAT, "Recorded stat %d, item %d, value %f", stat_id, item_id, value);

    log(LOG_STATS, "Stat %d.%u recorded", stat_id, item_id);
    return 0;
}

static int record_stat(enum stat_id stat_id, unsigned int item_id, double value) {
    int rtn = _record_stat(stat_id, item_id, value, true);
    return rtn;
}

DEF_INDIV_FNS_WARG(record_stat,
                   record_msu_stat, record_thread_stat, record_rt_stat,
                   double);

static int get_last_stat(enum stat_id stat_id, unsigned int item_id, double *out) {
    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    struct stat_item_group *group = get_item_group(type, item_id);
    if (group == NULL) {
        log_error("Canot get last stat %s for item id %u", type->stat.label, item_id);
        return -1;
    }

    *out = group->last_stat.value;
    return 0;
}
DEF_INDIV_FNS_WARG(get_last_stat,
                   last_msu_stat, last_thread_stat, last_rt_stat,
                   double *);


static int increment_stat(enum stat_id stat_id, unsigned int item_id, double increment) {
    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    struct stat_item_group *group = get_item_group(type, item_id);
    if (group == NULL) {
        log_error("Could not retrieve item group %d.%u", stat_id, item_id);
        return -1;
    }

    pthread_mutex_lock(&group->last_stat_lock);
    int rtn = _record_stat(stat_id, item_id, group->last_stat.value + increment, false);
    pthread_mutex_unlock(&group->last_stat_lock);
    return rtn;
}
DEF_INDIV_FNS_WARG(increment_stat,
                   increment_msu_stat, increment_thread_stat, increment_rt_stat,
                   double);

static int begin_interval(enum stat_id stat_id, unsigned int item_id) {
    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    struct stat_item_group *group = get_item_group(type, item_id);
    if (group == NULL) {
        return -1;
    }
    dur_timestamp(&group->last_start_time);
    return 0;
}
DEF_INDIV_FNS(begin_interval,
              begin_msu_interval, begin_thread_interval, begin_rt_interval)

static int end_interval(enum stat_id stat_id, unsigned int item_id) {
    struct timespec t;
    dur_timestamp(&t);

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }
    struct stat_item_group *group = get_item_group(type, item_id);
    if (group == NULL) {
        return -1;
    }
    double td = timediff_dbl(group->last_start_time, t);
    return record_stat(stat_id, item_id, td);
}
DEF_INDIV_FNS(end_interval,
              end_msu_interval, end_thread_interval, end_rt_interval);

static struct stat_item_group *allocate_stat_item_group(enum stat_id stat, unsigned int item_id) {
    struct stat_item_group *group = calloc(sizeof(*group), 1);
    if (group == NULL) {
        log_error("Failed to allocate stat_item_group");
        return NULL;
    }
    group->stat = stat;
    group->referent.id = id_from_item_id(item_id);
    group->referent.type = ITEM_ID_REFERENT_TYPE(item_id);
    log(LOG_STAT_ALLOCATION, "Allocated item  id=%d, item_id=%d, type=%d, stat=%d",
        group->referent.id, item_id, group->referent.type, stat);
    timestamp(&group->stat_list[0].start_time);

    pthread_mutex_init(&group->last_stat_lock, NULL);
    pthread_mutex_init(&group->write_index_lock, NULL);

    for (int i=0; i < ITEM_GROUP_SIZE; i++) {
        pthread_mutex_init(&group->stat_list[i].writer_mutex, NULL);
        pthread_mutex_init(&group->stat_list[i].record_lock, NULL);
    }
    return group;
}

static void free_stat_item_group(struct stat_item_group *group) {
    pthread_mutex_destroy(&group->last_stat_lock);
    pthread_mutex_destroy(&group->write_index_lock);

    for (int i=0; i < ITEM_GROUP_SIZE; i++) {
        pthread_mutex_destroy(&group->stat_list[i].writer_mutex);
        pthread_mutex_destroy(&group->stat_list[i].record_lock);
    }
    free(group);
}

static pthread_mutex_t all_sample_lock = PTHREAD_MUTEX_INITIALIZER;
static int total_num_stats;
static struct stat_sample *all_samples;

static int lock_samples() {
    if (pthread_mutex_lock(&all_sample_lock) != 0) {
        log_error("Error locking stat samples");
        return -1;
    }
    return 0;
}
static int unlock_samples() {
    if (pthread_mutex_unlock(&all_sample_lock) != 0) {
        log_error("Error unlocking stat samples");
        return -1;
    }
    return 0;
}

static int remove_stat_item(enum stat_id stat_id, unsigned int item_id) {
    ORDER_CHECK;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        return 0;
    }

    if (lock_type(type)) {
        return -1;
    }

    if (type->stat_items[item_id] == NULL) {
        log_error("Cannot remove item %s.%u -- already removed",
                  type->stat.label, item_id);
        unlock_type(type);
        return -1;
    }
    int i;
    for (i=0; i < MAX_ITEM_ID && type->used_ids[i] != item_id; i++);

    if (i == MAX_ITEM_ID) {
        log_warn("Cannot find id %u in stat %s", item_id, type->stat.label);
    }
    type->used_ids[i] = 0;

    free_stat_item_group(type->stat_items[item_id]);

    unlock_type(type);

    if (lock_samples()) {
        log_error("Could not lock mutex for samples");
        return -1;
    }
    total_num_stats--;
    all_samples = realloc(all_samples, sizeof(*all_samples) * total_num_stats * 2);
    unlock_samples();
    return 0;
}

DEF_INDIV_FNS(remove_stat_item,
              remove_msu_stat, remove_thread_stat, remove_rt_stat);


static int init_stat_item(enum stat_id stat_id, unsigned int item_id) {
    ORDER_CHECK;

    struct stat_type *type = &stat_types[stat_id];
    if (!type->enabled) {
        log(LOG_STATS, "Skipping initialization of type %s item %d",
            type->stat.label, item_id);
        return 0;
    }

    if (lock_samples()) {
        log_error("Could not lock mutex for samples");
        return -1;
    }
    total_num_stats++;
    all_samples = realloc(all_samples, sizeof(*all_samples) * total_num_stats * 2);
    unlock_samples();


    if (lock_type(type)) {
        return -1;
    }

    if (type->stat_items[item_id] != NULL) {
        // MAYBE: Log the rt, th, msu of the already-initialized stat
        log_error("Item ID %u already initialized for stat %s",
                  item_id, type->stat.label);
        unlock_type(type);
        return -1;
    }

    // Iterate through until an unused index is found
    int i;
    for (i=0; i < MAX_ITEM_ID && type->used_ids[i] != 0; i++);

    if (i == MAX_ITEM_ID) {
        log_error("Cannot fit more statistics of type %s", type->stat.label);
        unlock_type(type);
        return -1;
    }

    type->used_ids[i] = item_id;

    if (type->max_used_id_index < i) {
        type->max_used_id_index = i;
    }

    struct stat_item_group *group = allocate_stat_item_group(stat_id, item_id);;
    if (group == NULL) {
        return -1;
    }

    type->stat_items[item_id] = group;

    unlock_type(type);

    log(LOG_STATS, "Initialized stat item %s.%u",
        type->stat.label, item_id);

    return 0;
}
DEF_INDIV_FNS(init_stat_item,
              init_msu_stat, init_thread_stat, init_rt_stat);

int init_runtime_statistics() {
    log_info("Initialized rt stat %d", 12314123);
    for (int i=0; i < N_RUNTIME_STAT_TYPES; i++) {
        init_rt_stat(runtime_stat_types[i].id);
        log_info("Initialized rt stat %d", runtime_stat_types[i].id);
    }
    return 0;
}


static unsigned int qpartition(struct timed_stat *stats, int size) {
    double mid_val = stats[size - 1].value;
    int i = 0;

    struct timed_stat swap;
    for (int j = 0; j < size - 1; j++) {
        if (stats[j].value <= mid_val) {
            swap = stats[j];
            stats[j] = stats[i];
            stats[i] = swap;
            i++;
        }
    }
    swap = stats[size-1];
    stats[size-1] = stats[i];
    stats[i] = swap;
    return i;
}

static void qsort_by_value(struct timed_stat *stats, int size) {
    if (size > 1) {
        unsigned int idx = qpartition(stats, size);

        qsort_by_value(stats, idx);
        qsort_by_value(stats + idx + 1, size - idx - 1);
    }
}

static int sample_stat_item(struct stat_item_group *group, struct stat_sample *sample) {
    struct stat_list *read_list = &(group->stat_list[group->write_index]);

    pthread_mutex_lock(&read_list->record_lock);
    unsigned int size = read_list->write_index;
    if (size == 0 || read_list->recorded) {
        pthread_mutex_unlock(&read_list->record_lock);
        return 1;
    }

    read_list->recorded = true;
    read_list->write_index = 0;
    qsort_by_value(read_list->stats, size);


    sample->total_samples = size;

    double increment = (double) size / N_STAT_BINS;
    int max_bins = N_STAT_BINS;
    if (increment < 0) {
        max_bins = size;
        increment = 1;
    }
    if (read_list->stats[0].value == read_list->stats[size-1].value) {
        sample->bin_edges[0] = read_list->stats[0].value;
        sample->bin_edges[1] = read_list->stats[0].value;
        sample->bin_size[0] = size;
        sample->n_bins = 1;
    } else {
        int last_index = 0;
        int bin_index = 0;
        sample->bin_edges[0] = read_list->stats[0].value;
        for (int i=1; i<max_bins - 1; i++) {
            int sample_index = (int)(increment * i);
            double sample_value = read_list->stats[sample_index].value;

            sample->bin_size[bin_index] = sample_index - last_index;
            if (sample_value == sample->bin_edges[bin_index]) {
                continue;
            }
            sample->bin_edges[bin_index+1] = sample_value;
            last_index = sample_index;
            bin_index++;
        }
        sample->bin_edges[bin_index + 1] = read_list->stats[size - 1].value;
        sample->bin_size[bin_index] = size - last_index;
        sample->n_bins = bin_index + 1;
    }

    sample->start = read_list->start_time;
    timestamp(&sample->end);
    sample->stat_id = group->stat;
    sample->referent = group->referent;
    log(LOG_STAT_SAMPLES, "Referent: %d.%d", sample->referent.type, sample->referent.id);

    pthread_mutex_unlock(&read_list->record_lock);
    log(LOG_STAT_SAMPLES, "Sample min: %f max: %f",
            sample->bin_edges[0], sample->bin_edges[sample->n_bins - 1]);
    return 0;
}

static int sample_stat_type(struct stat_type *type,
                     struct stat_sample *samples,
                     unsigned int n_samples) {
    lock_type(type);
    int sample_index = 0;
    for (int i=0; i <= type->max_used_id_index; i++) {
        unsigned int id = type->used_ids[i];
        if (id == 0) {
            continue;
        }
        struct stat_item_group *group = type->stat_items[id];
        if (group == NULL) {
            log_warn("Group for id %u is null yet it persists in used_ids", id);
            continue;
        }
        if (sample_index >= n_samples) {
            log_warn("Out of samples to store statistic %s (n_samples=%d)", 
                     type->stat.label, n_samples);
            break;
        }
        int rtn = sample_stat_item(group, samples + sample_index);
        if (rtn == 0) {
            sample_index++;
        }
    }
    unlock_type(type);
    return sample_index;
}

int sample_stats(struct stat_sample **samples) {
    if (lock_samples()) {
        return -1;
    }
    *samples = all_samples;
    int samples_taken = 0;
    for (int i=0; i < N_REPORTED_STAT_TYPES; i++) {
        int rtn = sample_stat_type(&stat_types[reported_stat_types[i].id],
                                   all_samples + samples_taken,
                                   total_num_stats - samples_taken);
        if (rtn < 0) {
            log_error("Error sampling statistics");
            break;
        }
        samples_taken += rtn;
    }

    unlock_samples();
    return samples_taken;
}

int get_stat_limit(enum stat_id id, double *limit) {
    struct stat_type *type = &stat_types[id];
    if (type == NULL) {
        return -1;
    }
    if (type->limit_fn == NULL) {
        return 1;
    }
    return type->limit_fn(limit);
}
