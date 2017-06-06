#ifndef STATISTICS_H
#define STATISTICS_H
#include "stats.h"

#include <time.h>

#define TIME_SLOTS 60



struct timeserie {
    int data[TIME_SLOTS];
    time_t timestamp[TIME_SLOTS];
    short timepoint;
};

struct dfg_vertex;
enum stat_id;
int average(struct dfg_vertex *msu, enum stat_id stat_id);
#endif
