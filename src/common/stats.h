#ifndef STATS_H_
#define STATS_H_
#include <time.h>
#include <stdlib.h>

enum stat_id {
    MSU_QUEUE_LEN,
    MSU_ITEMS_PROCESSED,
    MSU_EXEC_TIME,
    MSU_IDLE_TIME,
    MSU_MEM_ALLOC,
    MSU_NUM_STATES,
    THREAD_CTX_SWITCHES,
    MSU_STAT1,
    MSU_STAT2,
    MSU_STAT3
};

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
    
struct stat_sample *init_stat_samples(int max_stats, int n_samples);

int deserialize_stat_samples(void *buffer, size_t buff_len, struct stat_sample *samples,
                             int n_samples);

ssize_t serialize_stat_samples(struct stat_sample *samples, int n_samples,
                               void *buffer, size_t buff_len);
#endif
