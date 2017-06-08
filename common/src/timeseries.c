#ifndef STATISTICS_H_
#define STATISTICS_H_

#include "timeseries.h"
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

int append_to_timeseries(struct timed_stat *input, int input_size,
                         struct circular_timeseries *ts) {
    int write_index = ts->write_index;
    for (int i=0; i<input_size; i++) {
        ts->data[write_index] = input[i].stat;
        ts->time[write_index] = input[i].time;
        write_index++;
        write_index %= TIME_SLOTS;
    }
    ts->write_index = write_index;
    return 0;
}

#define PRINT_LEN 6

void print_timeseries(struct circular_timeseries *ts){

    char str[TIME_SLOTS * 10 * 2];
    char *buff = str;

    buff += sprintf(buff, "TIME: ");

    for (int i=0; i<PRINT_LEN; i++) {
        int index = (ts->write_index + i) % TIME_SLOTS;
        double time = ts->time[index].tv_sec + ((double)ts->time[index].tv_nsec * 1e-9);
        buff += sprintf(buff, "| %9.4f ", time);
    }

    buff += sprintf(buff, "| ... ");

    for (int i=TIME_SLOTS-PRINT_LEN; i<TIME_SLOTS; i++) {
        int index = (ts->write_index + i) % TIME_SLOTS;
        double time = ts->time[index].tv_sec + ((double)ts->time[index].tv_nsec * 1e-9);
        buff += sprintf(buff, "| %9.4f ", time);
    }

    buff += sprintf(buff, "\nDATA: ");
    for (int i=0; i<PRINT_LEN; i++){
        int index = (ts->write_index + i) % TIME_SLOTS;
        buff += sprintf(buff, "| %9d ", (int)ts->data[index]);
    }
    buff += sprintf(buff, "| ... ");

    for (int i=TIME_SLOTS-PRINT_LEN; i<TIME_SLOTS; i++){
        int index = (ts->write_index + i) % TIME_SLOTS;
        buff += sprintf(buff, "| %9d ", (int)ts->data[index]);
    }
    buff += sprintf(buff, "\n");
    printf(str);
}
#endif //STATISTICS_H_
