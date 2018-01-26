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
#include "controller_dfg.h"
#include "stats.h"
#include "controller_stats.h"
#include "msu_ids.h"
#include "logging.h"
#include "haproxy.h"
#include "scheduling.h"

#include <stdbool.h>

#define MAX_MSU_TYPE_ID 9999

static int min_msu_types[MAX_MSU_TYPE_ID];

int cloneable_types[] = {
    WEBSERVER_READ_MSU_TYPE_ID,
    WEBSERVER_HTTP_MSU_TYPE_ID,
    WEBSERVER_REGEX_MSU_TYPE_ID
};

#define N_CLONEABLE_TYPES sizeof(cloneable_types) / sizeof(*cloneable_types)

int q_len_mins(struct dfg_msu_type *type, double range[2]) {
    bool recorded = false;
    for (int i=0; i < type->n_instances; i++) {
        struct dfg_msu *msu = type->instances[i];
        struct timed_rrdb *minstat = get_msu_stat(QUEUE_LEN, msu->id);
        struct timed_rrdb *maxstat = get_msu_max_stat(QUEUE_LEN, msu->id);
        if (!minstat->used || !maxstat->used) {
            continue;
        }
        double minval = minstat->data[(minstat->write_index + RRDB_ENTRIES - 1) % RRDB_ENTRIES];
        double maxval = maxstat->data[(maxstat->write_index + RRDB_ENTRIES - 1) % RRDB_ENTRIES];
        if (!recorded) {
            range[0] = minval;
            range[1] = maxval;
            recorded = true;
        } else {
            if (minval < range[0]) {
                range[0] = minval;
            }
            if (maxval < range[1]) {
                range[1] = maxval;
            }
        }
    }
    if (!recorded) {
        return -1;
    }
    return 0;
}

static struct timespec last_decision_time;

bool enough_time_elapsed(double periods) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if ((double)(now.tv_sec - last_decision_time.tv_sec) * 1e3 +
            (double)(now.tv_nsec * 1e-6 - last_decision_time.tv_nsec * 1e-6) > STAT_SAMPLE_PERIOD_MS * periods) {
        log_info("%f ms have elapsed", STAT_SAMPLE_PERIOD_MS * periods);
        return true;
    }
    return false;
}
int clone_type(struct dfg_msu_type *type) {
    clock_gettime(CLOCK_MONOTONIC, &last_decision_time);
    struct dfg_msu *cloned_msu = type->instances[0];
    struct dfg_msu *msu = clone_msu(cloned_msu->id);
    if (msu == NULL) {
        log_error("Cloning MSU failed for MSU type %d", type->id);
        return -1;
    }
    set_haproxy_weights(0,0);
    log(LOG_SCHEDULING_DECISIONS, "CLONING TYPE %d", type->id);
    return 0;
}

int try_to_clone_type(struct dfg_msu_type *type) {
    double mins[2];
    int rtn = q_len_mins(type, mins);
    if (rtn < 0) {
        return 0;
    }
    double minmin = mins[0];

    if (minmin > 0) {
        log(LOG_SCHEDULING_DECISIONS, "MINMIN is %d", (int)minmin);
        if (could_clone_type(type) && enough_time_elapsed(2)) {
            clone_type(type);
        }
    }
    return 0;
}


int try_to_clone() {
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);

    for (int i=0; i < N_CLONEABLE_TYPES; i++) {
        struct dfg_msu_type *msu_type = get_dfg_msu_type(cloneable_types[i]);
        try_to_clone_type(msu_type);
    }
    return 0;
}

int unclone_type(struct dfg_msu_type *type) {
    clock_gettime(CLOCK_MONOTONIC, &last_decision_time);
    struct dfg_msu *to_remove = type->instances[type->n_instances-1];
    int rtn = unclone_msu(to_remove->id);
    if (rtn < 0) {
        log(LOG_SCHEDULING, "Uncloning MSU failed");
        return -1;
    }
    set_haproxy_weights(0,0);
    log(LOG_SCHEDULING_DECISIONS, "UNCLONING TYPE %d", type->id);
    return 0;
}

int try_to_unclone_type(struct dfg_msu_type *type) {
    if (min_msu_types[type->id] >= type->n_instances) {
        return 0;
    }

    double mins[2];
    int rtn = q_len_mins(type, mins);
    if (rtn < 0) {
        return 0;
    }
    double minmax = mins[1];

    log(LOG_SCHEDULING_DECISIONS, "MINMAX IS %d", (int)minmax);
    if (minmax == 0 && enough_time_elapsed(4)) {
        unclone_type(type);
    }
    return 0;
}

int try_to_unclone() {
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);

    for (int i=0; i < N_CLONEABLE_TYPES; i++) {
        struct dfg_msu_type *msu_type = get_dfg_msu_type(cloneable_types[i]);
        try_to_unclone_type(msu_type);
    }
    return 0;
}
static bool min_instances_recorded = false;

int perform_cloning() {
    if (!min_instances_recorded) {
        struct dedos_dfg *dfg = get_dfg();
        for (int i=0; i < dfg->n_msu_types; i++) {
            struct dfg_msu_type *clone_type = dfg->msu_types[i];
            if (clone_type != NULL) {
                min_msu_types[clone_type->id] = clone_type->n_instances;
            }
        }
        min_instances_recorded = true;
    }
    try_to_clone();
    try_to_unclone();
    return 0;
}
