/**
 * timeseries.h
 *
 * Contains code relevant to storing and processing a round-robin database of timeseries
 */

#ifndef TIMESERIES_H
#define TIMESERIES_H
#include "stats.h"
#include <time.h>

/** The maximum length of the round-robin database */
#define RRDB_ENTRIES 60

/** Round-robin database (circular buffer) for storing timeseries data.
 * Meant for reporting statistics to the global controller */
struct timed_rrdb {
    double data[RRDB_ENTRIES];            /**< The statistics */
    struct timespec time[RRDB_ENTRIES];   /**< The time at which the stats were gathered */
    int write_index;     /**< Offset into the rrdb at which writing has occurred */
};

// Have to put this here to deal with ciruclar depedency :(
#include "dfg.h"

/** Calculates the average of a statistic for a given MSU */
int average(struct dfg_vertex *msu, enum stat_id stat_id);

/** Appends a number of timed statistics to a timeseries. */
int append_to_timeseries(struct timed_stat *input, int input_size,
                         struct timed_rrdb *timeseries);

/** Prints the beginning and end of a timeseries. */
void print_timeseries(struct timed_rrdb *ts);

#endif
