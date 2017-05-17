#ifndef STATISTICS_H
#define STATISTICS_H

#include "dfg.h"
#include <time.h>

#define TIME_SLOTS 60

struct timeserie {
    int data[TIME_SLOTS];
    time_t timestamp[TIME_SLOTS];
    short timepoint;
};

#endif
