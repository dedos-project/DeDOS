/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
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
#define RRDB_ENTRIES 240

/** Round-robin database (circular buffer) for storing timeseries data.
 * Meant for reporting statistics to the global controller */
struct timed_rrdb {
    double data[RRDB_ENTRIES];            /**< The statistics */
    struct timespec time[RRDB_ENTRIES];   /**< The time at which the stats were gathered */
    int write_index;     /**< Offset into the rrdb at which writing has occurred */
};

// Have to put this here to deal with ciruclar depedency :(
#include "dfg.h"

/** Calculates the average of the last n stats for a given MSU */
double average_n(struct timed_rrdb *timeseries, int n_samples);

/** Appends a number of timed statistics to a timeseries. */
int append_to_timeseries(struct timed_stat *input, int input_size,
                         struct timed_rrdb *timeseries);

/** Prints the beginning and end of a timeseries. */
void print_timeseries(struct timed_rrdb *ts);


#endif
