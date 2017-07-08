/**
 * timeseries.c
 *
 * Contains code relevant to storing and processing a round-robin database of timeseries
 */

#include "timeseries.h"
#include "stats.h"
#include "dfg.h"

struct timed_rrdb *get_timeseries(struct dfg_vertex *msu, enum stat_id stat_id) {
    switch (stat_id) {
        case QUEUE_LEN:
            return &msu->statistics.queue_length;
            break;
        case ITEMS_PROCESSED:
            return &msu->statistics.queue_items_processed;
            break;
        case MEMORY_ALLOCATED:
            return &msu->statistics.memory_allocated;
           break;
        default:
            debug("%s", "Unknown statistic");
    }
    return NULL;
}

double timeseries_min(struct dfg_vertex *msu, enum stat_id stat_id, int n_samples) {
    struct timed_rrdb *timeseries = get_timeseries(msu, stat_id);
    if (timeseries == NULL) {
        return -1;
    }

    double min = -1;
    for (int i=0; i<n_samples; i++) {
        int index = timeseries->write_index - i - 1;
        if (index < 0) {
            index = RRDB_ENTRIES + index;
        }
        if (timeseries->time[index].tv_sec == 0) {
            return min;
        }
        if (min < 0 || timeseries->data[index] < min) {
            min = timeseries->data[index];
        }
    }
    return min;
}

double stat_increase(struct dfg_vertex *msu, enum stat_id stat_id, int n_samples) {
    struct timed_rrdb *timeseries = get_timeseries(msu, stat_id);
    if (timeseries == NULL) {
        return -1;
    }

    int index = timeseries->write_index - 1;
    if (index < 0) {
        index = RRDB_ENTRIES + index;
    }
    if (timeseries->time[index].tv_sec == 0) {
        return 0;
    }
    double end = timeseries->data[index];
    index -= n_samples;
    if (index < 0) {
        index = RRDB_ENTRIES + index;
    }
    if (timeseries->time[index].tv_sec == 0) {
        return end;
    }
    return end - timeseries->data[index];
}

double average_n(struct dfg_vertex *msu, enum stat_id stat_id, int n_samples) {
    struct timed_rrdb *timeseries;
    switch (stat_id) {
        case QUEUE_LEN:
            timeseries = &msu->statistics.queue_length;
            break;
        case ITEMS_PROCESSED:
            timeseries = &msu->statistics.queue_items_processed;
            break;
        case MEMORY_ALLOCATED:
            timeseries = &msu->statistics.memory_allocated;
           break;
        default:
            debug("%s", "Unknown statistic");
            return -1;
    }

    double sum = -1;
    int i;
    int start_index = timeseries->write_index - 1;
    log_debug("Start index %d", start_index);
    for (i=0; i<n_samples; i++) {
        int index = start_index - i;
        if ( index < 0 ) 
            index = RRDB_ENTRIES + index;
        if ( timeseries->time[index].tv_sec == 0 ){
            log_debug("Break on %d" index);
            break;
        }
        if ( sum == -1 ) {
            sum = timeseries->data[index];
        } else {
            sum += timeseries->data[index];
        }
    }
    if (i==0)
        return -1;
    return sum / i;
}



/** Calculates the average of a statistic for a specific MSU.
 * @param msu The msu to which the statistics refer
 * @param stat_id The specific statstic to measure
 * @return An average of the reported statistics
 */
int average(struct dfg_vertex *msu, enum stat_id stat_id) {
    int i;
    int average = 0;
    int samples = 0;
    struct timed_rrdb *timeseries;

    if (msu == NULL) {
        debug("%s", "invalid MSU");
        return -1;
    }

    switch (stat_id) {
        case QUEUE_LEN:
            timeseries = &msu->statistics.queue_length;
            break;
        case ITEMS_PROCESSED:
            timeseries = &msu->statistics.queue_items_processed;
            break;
        case MEMORY_ALLOCATED:
            timeseries = &msu->statistics.memory_allocated;
           break;
        default:
            debug("%s", "Unknown statistic");
            return -1;
    }

    for (i = 0; i < RRDB_ENTRIES; ++i) {
        if (timeseries->time[i].tv_sec > 0) {
            average += timeseries->data[i];
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

/** Appends a number of timed statistics to a timeseries.
 * @param input The timed statistics to append to the timeseries
 * @param input_size The length of *input
 * @param timeseries The timeseries to which the data is to be appended
 * @return 0 on success
 * */
int append_to_timeseries(struct timed_stat *input, int input_size,
                         struct timed_rrdb *timeseries) {
    int write_index = timeseries->write_index;
    int tmp = RRDB_ENTRIES + 1;
    for (int i=0; i<input_size; i++) {
        timeseries->data[write_index] = input[i].stat;
        timeseries->time[write_index] = input[i].time;
        write_index++;
        write_index %= RRDB_ENTRIES;
    }
    timeseries->write_index = write_index;
    return 0;
}

/** The length of the begnning and end of the timeseries that's printed
 * when print_timeseries() is called */
#define PRINT_LEN 3

/** Prints the beginning and end of a timeseries.
 * @param timeseries The timeseries to print
 */
void print_timeseries(struct timed_rrdb *timeseries){

    char str[RRDB_ENTRIES * 10 * 2];
    char *buff = str;

    buff += sprintf(buff, "TIME: ");

    for (int i=0; i<PRINT_LEN; i++) {
        int index = (timeseries->write_index + i) % RRDB_ENTRIES;
        double time = timeseries->time[index].tv_sec +
                      ((double)timeseries->time[index].tv_nsec * 1e-9);
        buff += sprintf(buff, "| %9.4f ", time);
    }

    buff += sprintf(buff, "| ... ");

    for (int i=RRDB_ENTRIES-PRINT_LEN; i<RRDB_ENTRIES; i++) {
        int index = (timeseries->write_index + i) % RRDB_ENTRIES;
        double time = timeseries->time[index].tv_sec +
                      ((double)timeseries->time[index].tv_nsec * 1e-9);
        buff += sprintf(buff, "| %9.4f ", time);
    }

    buff += sprintf(buff, "\nDATA: ");
    for (int i=0; i<PRINT_LEN; i++){
        int index = (timeseries->write_index + i) % RRDB_ENTRIES;
        buff += sprintf(buff, "| %9d ", (int)timeseries->data[index]);
    }
    buff += sprintf(buff, "| ... ");

    for (int i=RRDB_ENTRIES-PRINT_LEN; i<RRDB_ENTRIES; i++){
        int index = (timeseries->write_index + i) % RRDB_ENTRIES;
        buff += sprintf(buff, "| %9d ", (int)timeseries->data[index]);
    }
    buff += sprintf(buff, "\n");
    printf("%s", str);
}
