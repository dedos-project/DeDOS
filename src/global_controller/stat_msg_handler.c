#include "stat_msg_handler.h"
#include "timeseries.h"
#include "msu_stats.h"
#include "scheduling_decision.h"
#include "logging.h"
#include "scheduling.h"
#include "controller_dfg.h"
#include "controller_mysql.h"

#define MAX_STAT_SAMPLES 128
#define MAX_SAMPLE_SIZE 64
struct stat_sample *incoming_samples;

static int process_stat_sample(struct stat_sample *sample) {
    struct timed_rrdb *stat = get_stat(sample->hdr.stat_id, sample->hdr.item_id);
    if (stat == NULL) {
        //log_error("Error processing stat sample %d.%u", sample->hdr.stat_id, sample->hdr.item_id);
        return -1;
    }
    int rtn = append_to_timeseries(sample->stats, sample->hdr.n_stats, stat);
    if (rtn < 0) {
        log_error("Error appending stats %d.%u to timeseries",
                  sample->hdr.stat_id, sample->hdr.item_id);
        return -1;
    }

    rtn = db_insert_sample(sample->stats, &sample->hdr);
    if (rtn < 0) {
        log_error("Error inserting stats %d.%u into DB",
                  sample->hdr.stat_id, sample->hdr.item_id);
        return -1;
    }

    return 0;
}

int handle_serialized_stats_buffer(void *buffer, size_t buffer_len) {
    if (incoming_samples == NULL) {
        log_error("Stat msg handler not initialized");
        return -1;
    }

    int n_samples = deserialize_stat_samples(buffer, buffer_len,
                                             incoming_samples, MAX_STAT_SAMPLES);
    if (n_samples < 0) {
        log_error("Error deserializing stat samples");
        return -1;
    }

    for (int i=0; i<n_samples; i++) {
        if (process_stat_sample(&incoming_samples[i]) != 0) {
            //log_error("Error processing stat sample");
        }
    }

#ifdef DYN_SCHED
    perform_cloning();
    fix_all_route_ranges(get_dfg());
#endif

    return 0;
}

int init_stats_msg_handler() {
    incoming_samples = init_stat_samples(MAX_STAT_SAMPLES, MAX_SAMPLE_SIZE);
    if (incoming_samples == NULL) {
        log_error("Error initializing stat samples");
        return -1;
    }
    int rtn = init_statistics();
    if (rtn < 0) {
        log_error("Error initializing statistics");
        return -1;
    }
    return 0;
}
