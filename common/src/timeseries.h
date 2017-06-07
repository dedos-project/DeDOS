#ifndef STATISTICS_H
#define STATISTICS_H
#include "stats.h"

#include <time.h>

#define TIME_SLOTS 60

struct circular_timeseries {
    double data[TIME_SLOTS];
    struct timespec time[TIME_SLOTS];
    int write_index;
};

struct timeserie {
    int data[TIME_SLOTS];
    time_t timestamp[TIME_SLOTS];
    short timepoint;
};

struct dfg_vertex;
int average(struct dfg_vertex *msu, enum stat_id stat_id);

int append_to_timeseries(struct timed_stat *input, int input_size,
                         struct circular_timeseries *ts);
void print_timeseries(struct circular_timeseries *ts);

#endif
