/**
 * @file stats.h
 *
 * Functions for the sending and receiving of statistics between ctrl and runtime
 */

#ifndef STATS_H_
#define STATS_H_
#include "unused_def.h"
#include "stat_ids.h"

#include <time.h>
#include <stdlib.h>

/**
 * Holds a single timestamped value
 */
struct timed_stat {
    struct timespec time;
    long double value;
};

/** Header for a single stat sample for a single item */
struct stat_sample_hdr {
    /** The ID of the statistic being sampled */
    enum stat_id stat_id;
    /** The ID for the item being sampled */
    unsigned int item_id;
    /** The size of the sample (number of stats, not number of items) */
    int n_stats;
};

/** A single stat sample for a single item */
struct stat_sample {
    struct stat_sample_hdr hdr;
    /** The allocated size of the stat_sample::stats structure */
    int max_stats;
    /** The statistics in question*/
    struct timed_stat *stats;
};

/** Structure to hold both the stat ID and the string describing it */
struct stat_type_label {
    enum stat_id id;
    char *name;
};

#ifndef REPORTED_MSU_STAT_TYPES 
#define REPORTED_MSU_STAT_TYPES \
    {MSU_QUEUE_LEN, "QUEUE_LEN"}, \
    {MSU_ITEMS_PROCESSED, "ITEMS_PROCESSED"}, \
    {MSU_MEM_ALLOC, "MEMORY_ALLOCATED"}, \
    {MSU_NUM_STATES, "NUM_STATES"}, \
    {MSU_EXEC_TIME, "EXEC_TIME"}, \
    {MSU_IDLE_TIME, "IDLE_TIME"}
#endif 

#ifndef REPORTED_THREAD_STAT_TYPES 
#define REPORTED_THREAD_STAT_TYPES \
    {THREAD_UCPUTIME, "USER_TIME"}, \
    {THREAD_SCPUTIME, "KERNAL_TIME"}, \
    {THREAD_MAXRSS, "MAX_RSS"}, \
    {THREAD_MINFLT, "MINOR_FAULTS"}, \
    {THREAD_MAJFLT, "MAJOR_FAULTS"}, \
    {THREAD_VCSW, "VOL_CTX_SW"}, \
    {THREAD_IVCSW, "INVOL_CTX_SW"}
#endif 

#ifndef REPORTED_STAT_TYPES
#define REPORTED_STAT_TYPES \
    REPORTED_MSU_STAT_TYPES, \
    REPORTED_THREAD_STAT_TYPES 
#endif

int is_thread_stat(enum stat_id id);
int is_msu_stat(enum stat_id id);

/** Static structure so the reported stat types can be referenced as an array */
static struct stat_type_label UNUSED reported_stat_types[] = {
    REPORTED_STAT_TYPES
};
/** Number of reported stat types */
#define N_REPORTED_STAT_TYPES sizeof(reported_stat_types) / sizeof(*reported_stat_types)

static struct stat_type_label UNUSED reported_msu_stat_types[] = {
    REPORTED_MSU_STAT_TYPES
};
#define N_REPORTED_MSU_STAT_TYPES \
    sizeof(reported_msu_stat_types) / sizeof(*reported_msu_stat_types)

static struct stat_type_label UNUSED reported_thread_stat_types[] = {
    REPORTED_THREAD_STAT_TYPES
};
#define N_REPORTED_THREAD_STAT_TYPES \
    sizeof(reported_thread_stat_types) / sizeof(*reported_thread_stat_types)



/** Maxmimum identifier that can be assigned to a stat item */
#define MAX_STAT_ITEM_ID 4192

/** Number of statistics sampled in each send from runtime to controller */
#define STAT_SAMPLE_SIZE 5

/** How often samples are sent from runtime to controller */
#define STAT_SAMPLE_PERIOD_MS 500

/** Frees a set of stat samples */
void free_stat_samples(struct stat_sample *samples, int n_samples);

/**
 * Initilizes `n` sets of  samples of statistics, each of which contains `max_stats` points
 * @return allocated structure on success, NULL on error
 */
struct stat_sample *init_stat_samples(int max_stats, int n_samples);

/**
 * Deserializes from the provided buffer into the `samples` structure.
 * @param buffer The buffer to deserialize
 * @param buff_len The size of the serialized buffer
 * @param samples The structure into which to deserialize
 * @param n_samples The number of items allocated in `samples`
 * @return 0 on success, -1 on error
 */
int deserialize_stat_samples(void *buffer, size_t buff_len, struct stat_sample *samples,
                             int n_samples);

/**
 * Serializes from the provided `samples` into the `buffer`
 * @param samples The samples to deserialize
 * @param n_samples size of `samples`
 * @param buffer The buffer into which to serialize
 * @param buff_len The size of the allocated `buffer`
 * @return 0 on success, -1 on error (if buffer is too small)
 */
ssize_t serialize_stat_samples(struct stat_sample *samples, int n_samples,
                               void *buffer, size_t buff_len);

/**
 * Determines the size needed to hold the serialized version of `sample`.
 *
 * @param sample The sample to serialize
 * @param n_samples The number of elements in `sample`
 * @return size needed
 */
size_t serialized_stat_sample_size(struct stat_sample *sample, int n_samples);
#endif
