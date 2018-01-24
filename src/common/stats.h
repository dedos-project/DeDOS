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
 * @file stats.h
 *
 * Functions for the sending and receiving of statistics between ctrl and runtime
 */
#ifndef STATS_H_
#define STATS_H_
#include "unused_def.h"
#include "stat_ids.h"

#include <time.h>
#include <stdlib.h>

/**
 * Holds a single timestamped value
 */
struct timed_stat {
    struct timespec time;
    long double value;
};

enum stat_referent_type {
    MSU_STAT, THREAD_STAT, RT_STAT
};

struct stat_referent {
    unsigned int id;
    enum stat_referent_type type;
};

#define N_STAT_BINS 20
struct stat_sample {
    enum stat_id stat_id;
    struct stat_referent referent;
    struct timespec start;
    struct timespec end;
    unsigned int n_bins;
    double bin_edges[N_STAT_BINS + 1];
    unsigned int bin_size[N_STAT_BINS];
    unsigned int total_samples;
};

struct stat_limit {
    enum stat_id id;
    double limit;
};

#define MSU_STAT_TYPES \
    _STNAM(MEM_ALLOC), \
    _STNAM(FILEDES), \
    _STNAM(ERROR_CNT), \
    _STNAM(QUEUE_LEN), \
    _STNAM(ITEMS_PROCESSED), \
    _STNAM(EXEC_TIME), \
    _STNAM(IDLE_TIME), \
    _STNAM(NUM_STATES), \
    _STNAM(UCPUTIME), \
    _STNAM(SCPUTIME), \
    _STNAM(MINFLT), \
    _STNAM(MAJFLT), \
    _STNAM(VCSW), \
    _STNAM(IVCSW), \
    _STNAM(MSU_STAT1), \
    _STNAM(MSU_STAT2), \
    _STNAM(MSU_STAT3)

#define THREAD_STAT_TYPES \
    _STNAM(UCPUTIME), \
    _STNAM(FILEDES), \
    _STNAM(SCPUTIME), \
    _STNAM(MAXRSS), \
    _STNAM(MINFLT), \
    _STNAM(MAJFLT), \
    _STNAM(VCSW), \
    _STNAM(IVCSW),

#define RUNTIME_STAT_TYPES \
    _STNAM(FILEDES)

#define REPORTED_STAT_TYPES \
    _STNAM(FILEDES), \
    _STNAM(QUEUE_LEN), \
    _STNAM(ITEMS_PROCESSED), \
    _STNAM(EXEC_TIME), \
    _STNAM(IDLE_TIME), \
    _STNAM(MEM_ALLOC), \
    _STNAM(NUM_STATES), \
    _STNAM(ERROR_CNT), \
    _STNAM(UCPUTIME), \
    _STNAM(SCPUTIME), \
    _STNAM(MAXRSS), \
    _STNAM(MINFLT), \
    _STNAM(MAJFLT), \
    _STNAM(VCSW), \
    _STNAM(IVCSW), \
    _STNAM(MSU_STAT1), \
    _STNAM(MSU_STAT2), \
    _STNAM(MSU_STAT3),

/** Static structure so the reported stat types can be referenced as an array */
static struct stat_label UNUSED reported_stat_types[] = {
    REPORTED_STAT_TYPES
};

/** Number of reported stat types */
#define N_REPORTED_STAT_TYPES sizeof(reported_stat_types) / sizeof(*reported_stat_types)

static struct stat_label UNUSED msu_stat_types[] = {
    MSU_STAT_TYPES
};

#define N_MSU_STAT_TYPES sizeof(msu_stat_types) / sizeof(*msu_stat_types)

static struct stat_label UNUSED thread_stat_types[] = {
    THREAD_STAT_TYPES
};

#define N_THREAD_STAT_TYPES sizeof(thread_stat_types) / sizeof(*thread_stat_types)

static struct stat_label UNUSED runtime_stat_types[] = {
    RUNTIME_STAT_TYPES
};

#define N_RUNTIME_STAT_TYPES sizeof(runtime_stat_types) / sizeof(*runtime_stat_types)

/** How often samples are sent from runtime to controller */
#define STAT_SAMPLE_PERIOD_MS 200

size_t serialize_stat_samples(struct stat_sample *samples, int n_samples,
                              void **buffer);

int deserialize_stat_samples(void *buffer, size_t buff_len,
                             struct stat_sample **samples);


#endif
