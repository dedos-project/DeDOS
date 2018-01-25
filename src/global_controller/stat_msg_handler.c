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
#include "stat_msg_handler.h"
#include "timeseries.h"
#include "controller_stats.h"
#include "scheduling_decision.h"
#include "logging.h"
#include "scheduling.h"
#include "controller_dfg.h"
#include "controller_mysql.h"

#define MAX_STAT_SAMPLES 128
#define MAX_SAMPLE_SIZE 64
struct stat_sample *incoming_samples;

static int process_stat_samples(int runtime_id, int n_samples,
                               struct stat_sample *samples) {
    log(LOG_PROCESS_STATS, "Processing %d stats from runtime %d", n_samples, runtime_id);
    int rtn = db_insert_samples(samples, n_samples, runtime_id);
    if (rtn < 0) {
        log(LOG_MYSQL,"Error inserting stats into DB");
    }

    for (int i=0; i < n_samples; i++) {
        append_stat_sample(&samples[i], runtime_id);
    }

    return 0;
}

int set_rt_stat_limit(int runtime_id, struct stat_limit *lim) {
    return db_set_rt_stat_limit(runtime_id, lim->id, lim->limit);
}

int handle_serialized_stats_buffer(int runtime_id, void *buffer, size_t buffer_len) {
    if (incoming_samples == NULL) {
        log_error("Stat msg handler not initialized");
        return -1;
    }

    struct stat_sample *incoming;
    int n_samples = deserialize_stat_samples(buffer, buffer_len, &incoming);
    if (n_samples < 0) {
        log_error("Error deserializing stat samples");
        return -1;
    }

    process_stat_samples(runtime_id, n_samples, incoming);

#ifdef DYN_SCHED
    perform_cloning();
    fix_all_route_ranges(get_dfg());
#endif

    return 0;
}

int init_stats_msg_handler() {
    incoming_samples = malloc(sizeof(*incoming_samples) & MAX_STAT_SAMPLES);
    if (incoming_samples == NULL) {
        log_error("Error initializing stat samples");
        return -1;
    }
    return 0;
}
