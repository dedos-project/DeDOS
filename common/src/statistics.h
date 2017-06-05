#ifndef STATISTICS_H
#define STATISTICS_H
#include "stats.h"

#include <time.h>

#define TIME_SLOTS 60

/**
 * Structure to hold a single timestamped statistic
 */
struct timed_stat {
    double stat;
    struct timespec time;
};

/**
 * Structure to hold a series of timestamped statistics
 */
struct stat_sample {
    enum stat_id stat_id;
    int item_id;
    struct timespec cur_time;
    int n_stats;
    struct timed_stat stats;
}


struct timeserie {
    int data[TIME_SLOTS];
    time_t timestamp[TIME_SLOTS];
    short timepoint;
};

struct dfg_vertex;
enum stat_id;
int average(struct dfg_vertex *msu, enum stat_id stat_id);
#endif
