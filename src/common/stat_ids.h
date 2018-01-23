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
 * @file stat_ids.h
 * Declares the identifiers with which stats can be logged
 */
#ifndef STATS_IDS_H_
#define STATS_IDS_H_
#include "unused_def.h"
#include <time.h>
#include <stdlib.h>

enum stat_id {
    QUEUE_LEN,
    ITEMS_PROCESSED,
    EXEC_TIME,
    IDLE_TIME,
    MEM_ALLOC,
    NUM_STATES,
    ERROR_CNT,

    UCPUTIME,
    SCPUTIME,
    MAXRSS,
    MINFLT,
    MAJFLT,
    VCSW,
    IVCSW,

    MSU_STAT1,
    MSU_STAT2,
    MSU_STAT3,

    /** Profiling */
    PROF_ENQUEUE,
    /** Profiling */
    PROF_DEQUEUE,
    /** Profiling */
    PROF_REMOTE_SEND,
    /** Profiling */
    PROF_REMOTE_RECV,
    /** Profiling */
    PROF_DEDOS_ENTRY,
    /** Profiling */
    PROF_DEDOS_EXIT
};

struct stat_label {
    enum stat_id id;
    char *label;
};

#define _STNAM(stat) {stat, #stat}

static struct stat_label UNUSED stat_labels[] = {
    _STNAM(QUEUE_LEN),
    _STNAM(ITEMS_PROCESSED),
    _STNAM(EXEC_TIME),
    _STNAM(IDLE_TIME),
    _STNAM(MEM_ALLOC),
    _STNAM(NUM_STATES),
    _STNAM(ERROR_CNT),
    _STNAM(UCPUTIME),
    _STNAM(SCPUTIME),
    _STNAM(MAXRSS),
    _STNAM(MINFLT),
    _STNAM(MAJFLT),
    _STNAM(VCSW),
    _STNAM(IVCSW),
    _STNAM(MSU_STAT1),
    _STNAM(MSU_STAT2),
    _STNAM(MSU_STAT3),
    _STNAM(PROF_ENQUEUE),
    _STNAM(PROF_DEQUEUE),
    _STNAM(PROF_REMOTE_SEND),
    _STNAM(PROF_REMOTE_RECV),
    _STNAM(PROF_DEDOS_ENTRY),
    _STNAM(PROF_DEDOS_EXIT)
};

#endif
