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
