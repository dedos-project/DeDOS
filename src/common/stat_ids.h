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
