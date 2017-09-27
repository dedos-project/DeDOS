#ifndef STATS_H_
#define STATS_H_
#include "unused_def.h"
#include "stat_ids.h"
#include <time.h>
#include <stdlib.h>

struct timed_stat {
    struct timespec time;
    double value;
};

struct stat_sample_hdr {
    enum stat_id stat_id;
    unsigned int item_id;
    int n_stats;
};

struct stat_sample {
    struct stat_sample_hdr hdr;
    int max_stats;
    struct timed_stat *stats;
};

struct stat_type_label {
    enum stat_id id;
    char *name;
};

#ifndef REPORTED_STAT_TYPES
#define REPORTED_STAT_TYPES \
    {MSU_QUEUE_LEN, "QUEUE_LEN"}, \
    {MSU_ITEMS_PROCESSED, "ITEMS_PROCESSED"}, \
    {MSU_MEM_ALLOC, "MEMORY_ALLOCATED"}
#endif

static struct stat_type_label UNUSED reported_stat_types[] = {
    REPORTED_STAT_TYPES
};

#define N_REPORTED_STAT_TYPES sizeof(reported_stat_types) / sizeof(*reported_stat_types)

#define MAX_STAT_ITEM_ID 64

#define STAT_SAMPLE_SIZE 10

#define STAT_SAMPLE_PERIOD_S 1

void free_stat_samples(struct stat_sample *samples, int n_samples);

struct stat_sample *init_stat_samples(int max_stats, int n_samples);

int deserialize_stat_samples(void *buffer, size_t buff_len, struct stat_sample *samples,
                             int n_samples);

ssize_t serialize_stat_samples(struct stat_sample *samples, int n_samples,
                               void *buffer, size_t buff_len);

size_t serialized_stat_sample_size(struct stat_sample *sample, int n_samples);
#endif
