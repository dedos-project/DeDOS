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
