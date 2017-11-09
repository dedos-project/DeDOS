/**
 * @file profiler.h
 *
 * For profiling the path of MSU messages through DeDOS.
 *
 * Note: #DEDOS_PROFILER must be defined for profiling to do anything!
 */
#ifndef PROFILER_H__
#define PROFILER_H__
#include "msu_message.h"
#include "stat_ids.h"
#include "rt_stats.h"

/** The ID with which all profiled messages are tagged in the stats module */
#define PROFILER_ITEM_ID 101

/**
 * Sets the profiling flag in the header based on the tag_probability provided
 * to ::init_profiler
 */
void set_profiling(struct msu_msg_hdr *hdr);
/**
 * Sets the probability of profiling an MSU message
 */
void init_profiler(float tag_probability);

#ifdef DEDOS_PROFILER

/** Sets whether the header should be profiled. Ignored if #DEDOS_PROFILE is not defined */
#define SET_PROFILING(hdr) set_profiling(&hdr)

/** Initializes the profiler with the provided probability of profiling.
 * Ignored if #DEDOS_PROFILE is not defined */
#define INIT_PROFILER(tprof) init_profiler(tprof)

/**
 * If the header is marked for profiling, profiles the given event.
 * Ignored if #DEDOS_PROFILE is not defined.
 */
#define PROFILE_EVENT(hdr, stat_id) \
    if (hdr.do_profile && hdr.key.id != 0) \
        record_stat(stat_id, PROFILER_ITEM_ID, (double)hdr.key.id, true)
#else

/** Sets whether the header should be profiled. Ignored if #DEDOS_PROFILE is not defined */
#define SET_PROFILING(hdr)

/** Initializes the profiler with the provided probability of profiling.
 * Ignored if #DEDOS_PROFILE is not defined */
#define INIT_PROFILER(tprof) \
    if (tprof > 0) \
        log_warn("Profiling probability set to %f but profiling is disabled", tprof)

/**
 * If the header is marked for profiling, profiles the given event.
 * Ignored if #DEDOS_PROFILE is not defined.
 */
#define PROFILE_EVENT(hdr, stat_id)

#endif // DEDOS_PROFILER

#endif // PROFILER_H__
