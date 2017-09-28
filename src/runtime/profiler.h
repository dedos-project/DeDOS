#ifndef PROFILER_H__
#define PROFILER_H__
#include "msu_message.h"
#include "stat_ids.h"
#include "rt_stats.h"

#define PROFILER_ITEM_ID 101

void set_profiling(struct msu_msg_hdr *hdr);
void init_profiler(float tag_probability);
#ifdef DEDOS_PROFILER

#define SET_PROFILING(hdr) set_profiling(hdr)

#define INIT_PROFILER(tprof) init_profiler(tprof)

#define PROFILE_EVENT(hdr, stat_id) \
    if (hdr->do_profile && hdr->key.id != 0) \
        record_stat(stat_id, PROFILER_ITEM_ID, (double)hdr->key.id, true)
#else

#define SET_PROFILING(hdr)
#define INIT_PROFILER(tprof) \
    if (tprof > 0) \
        log_warn("Profiling probability set to %f but profiling is disabled", tprof)
#define PROFILE_EVENT(hdr, stat_id)

#endif // DEDOS_PROFILER

#endif // PROFILER_H__
