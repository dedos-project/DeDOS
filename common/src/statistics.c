#ifndef STATISTICS_H_
#define STATISTICS_H_

#include "statistics.h"
#include "stats.h"
#include "dfg.h"

int average(struct dfg_vertex *msu, enum stat_id stat_id) {
    int i;
    int average = 0;
    int samples = 0;
    struct timeserie TS;

    if (msu == NULL) {
        debug("%s", "invalid MSU");
        return -1;
    }

    switch (stat_id) {
        case QUEUE_LEN:
            TS = msu->statistics.data_queue_size;
            break;
        case ITEMS_PROCESSED:
            TS = msu->statistics.queue_item_processed;
            break;
        case MEMORY_ALLOCATED:
            TS = msu->statistics.memory_allocated;
           break;
        default:
            debug("%s", "Unknown statistic");
            return -1;
    }

    for (i = 0; i < TIME_SLOTS; ++i) {
        if (TS.timestamp[i] > 0) {
            average += TS.data[i];
            samples++;
        }
    }

    if (samples > 0) {
        average = average / samples;
        return average;
    } else {
        return 0;
    }
}

#endif //STATISTICS_H_
