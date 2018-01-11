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
 * @file profiler.c
 *
 * For profiling the path of MSU messages through DeDOS
 */
#include "profiler.h"
#include "stats.h"
#include "logging.h"
#include "rt_stats.h"

/** The probability that an MSU message will get marked for profiling */
static float tag_probability = -1;

/** The stat IDs relevant to profiling */
enum stat_id profiler_stat_ids[] = {
    PROF_ENQUEUE,
    PROF_DEQUEUE,
    PROF_REMOTE_SEND,
    PROF_REMOTE_RECV,
    PROF_DEDOS_ENTRY,
    PROF_DEDOS_EXIT
};
#define N_PROF_STAT_IDS sizeof(profiler_stat_ids) / sizeof(*profiler_stat_ids)

/** Sets the profiling flag on the header based on the ::tag_probability */
void set_profiling(struct msu_msg_hdr *hdr) {
#ifdef DEDOS_PROFILER
    float r = (float)rand() / (float)RAND_MAX;
    if (r > tag_probability) {
        hdr->do_profile = false;
        return;
    }
    hdr->do_profile = true;
#endif
    return;
}

/** Initializes the profiler to tag headers with tag_prob probability. */
void init_profiler(float tag_prob) {
    if (tag_probability != -1) {
        log_warn("Profiler initialized twice!");
    }
    tag_probability = tag_prob;
    srand(time(NULL));

    for (int i=0; i<N_PROF_STAT_IDS; i++) {
        init_stat_item(profiler_stat_ids[i], PROFILER_ITEM_ID);
    }
}
