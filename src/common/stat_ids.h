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
#include <time.h>
#include <stdlib.h>

/**
 * The identifiers with which stats can be logged
 */
enum stat_id {
    MSU_QUEUE_LEN,
    MSU_ITEMS_PROCESSED,
    MSU_EXEC_TIME,
    MSU_IDLE_TIME,
    MSU_MEM_ALLOC,
    MSU_NUM_STATES,
    MSU_ERROR_CNT,
    MSU_UCPUTIME,
    MSU_SCPUTIME,
    MSU_MINFLT,
    MSU_MAJFLT,
    MSU_VCSW,
    MSU_IVCSW,
    THREAD_UCPUTIME,
    THREAD_SCPUTIME,
    THREAD_MAXRSS,
    THREAD_MINFLT,
    THREAD_MAJFLT,
    THREAD_VCSW,
    THREAD_IVCSW,
    MSU_STAT1, /**< For custom MSU statistics */
    MSU_STAT2, /**< For custom MSU statistics */
    MSU_STAT3, /**< For custom MSU statistics */

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

#endif
