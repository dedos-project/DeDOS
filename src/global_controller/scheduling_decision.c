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
    SOCKET_MSU_TYPE_ID,
    WEBSERVER_READ_MSU_TYPE_ID,
    WEBSERVER_REGEX_MSU_TYPE_ID
};

#define N_CLONEABLE_TYPES sizeof(cloneable_types) / sizeof(*cloneable_types)

static double rt_min_min_gt_0(struct dfg_msu_type *type) {

    double min_qlens[MAX_RUNTIMES + 1];
    for (int i=0; i < MAX_RUNTIMES + 1; i++) {
        min_qlens[i] = -1;
    }

    for (int i=0; i < type->n_instances; i++) {
        struct dfg_msu *msu = type->instances[i];
        struct timed_rrdb *minstat = get_msu_stat(QUEUE_LEN, msu->id);
        if (!minstat->used) {
            continue;
        }
        double minval = minstat->data[(minstat->write_index + RRDB_ENTRIES - 1) % RRDB_ENTRIES];
        int rt = msu->scheduling.runtime->id;

        if (min_qlens[rt] > minval || min_qlens[rt] == -1) {
            min_qlens[rt] = minval;
        }
    }
    for (int i=0; i < MAX_RUNTIMES + 1; i++) {
        if (min_qlens[i] > 0) {
            return true;
        }
    }
    return false;
}

static double p_qlen_max_eq_0(struct dfg_msu_type *type, struct dfg_runtime *rt) {
    int n_instances = 0;
    int n_eq_0 = 0;
    for (int i=0; i < type->n_instances; i++) {
        struct dfg_msu *msu = type->instances[i];
        if (msu->scheduling.runtime != rt) {
            continue;
        }
        n_instances++;
        struct timed_rrdb *maxstat = get_msu_stat(QUEUE_LEN, msu->id);
        if (!maxstat->used) {
            continue;
        }
        double maxval = maxstat->data[(maxstat->write_index + RRDB_ENTRIES - 1) % RRDB_ENTRIES];
        if (maxval == 0) {
            n_eq_0 += 1;
        }
    }
    return (double)n_eq_0 / (double)n_instances;
}

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

double type_stat_contribution(struct dfg_msu_type *type, enum stat_id stat_id) {
    double contribution = 0;
    for (int i=0; i < type->n_instances; i++) {
        struct dfg_msu *msu = type->instances[i];
        struct timed_rrdb *stat = get_msu_stat(stat_id, msu->id);
        contribution += stat->data[(stat->write_index + RRDB_ENTRIES - 1) % RRDB_ENTRIES];
    }
    return contribution;
}


static bool reaching_runtime_limit(struct dfg_msu_type *type, bool any) {
    for (int i = 0; i < N_REPORTED_STAT_TYPES; i++) {
        enum stat_id stat_id = reported_stat_types[i].id;
        bool should_clone = false;
        for (int rt_i=0; rt_i < MAX_RUNTIMES; rt_i++) {
            double lim;
            int rtn = get_rt_stat_limit(rt_i, stat_id, &lim);
            if (rtn != 0 || lim <= 0) {
                continue;
            }
            struct timed_rrdb *stat = get_runtime_stat(stat_id, rt_i);
            if (stat == NULL || stat->used == 0) {
                continue;
            }
            bool has_on_runtime = false;
            for (int j=0; j < type->n_instances; j++) {
                if (type->instances[j]->scheduling.runtime->id == rt_i) {
                    has_on_runtime = true;
                }
            }
            if (!has_on_runtime) {
                continue;
            }

            double last_stat = stat->data[(stat->write_index + RRDB_ENTRIES - 1) % RRDB_ENTRIES];

            if (last_stat < .4 * lim) {
               should_clone = false;
               if (!any) {
                   break;
               }
               continue;
            }

            double type_contribution = type_stat_contribution(type, stat_id);
            if (type_contribution > .5 * last_stat) {
                log(LOG_RT_LIMITS, "rt %d type contr. = %f, current = %f, lim = %f", 
                    rt_i, type_contribution, last_stat, lim);
                should_clone = true;
                if (any) {
                    return true;
                }
            } else {
                if (!any) {
                    should_clone = false;
                    break;
                }
            }
        }
        if (should_clone) {
            return true;
        }
    }
    return false;
}


bool enough_time_elapsed(struct timespec *times, struct dfg_msu_type *type, double periods) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    struct timespec *last_time = NULL;
    for (int i=0; i < N_CLONEABLE_TYPES; i++) {
        if (cloneable_types[i] == type->id) {
            last_time = &times[i];
            break;
        }
    }
    if (last_time == NULL) {
        return false;
    }
    if ((double)(now.tv_sec - last_time->tv_sec) * 1e3 +
            (double)(now.tv_nsec * 1e-6 - last_time->tv_nsec * 1e-6) > STAT_SAMPLE_PERIOD_MS * periods) {
        return true;
    }
    return false;
}

static struct timespec last_clone_time[N_CLONEABLE_TYPES];
static struct timespec last_try_clone_time[N_CLONEABLE_TYPES];

int clone_type(struct dfg_msu_type *type) {
    struct dfg_msu *cloned_msu = type->instances[0];
    struct dfg_msu *msu = clone_msu(cloned_msu->id);
    if (msu == NULL) {
        log_error("Cloning MSU failed for MSU type %d", type->id);
        return -1;
    }
    set_haproxy_weights();
    log(LOG_SCHEDULING_DECISIONS, "CLONING TYPE %d", type->id);
    return 0;
}

void set_type_time(struct dfg_msu_type *type, struct timespec *time) {
    for (int i=0; i < N_CLONEABLE_TYPES; i++) {
        if (cloneable_types[i] == type->id) {
            clock_gettime(CLOCK_MONOTONIC, &time[i]);
        }
    }
}

void set_related_type_times(struct dfg_msu_type *type, struct timespec *time) {
    set_type_time(type, time);
    for (int i=0; i < type->n_dependencies; i++) {
        set_type_time(type->dependencies[i]->type, time);
    }
}

int try_to_clone_type(struct dfg_msu_type *type) {
    bool gt_0 = rt_min_min_gt_0(type);
    if (gt_0 || reaching_runtime_limit(type, false)) {
        set_related_type_times(type, last_try_clone_time);
        if (could_clone_type(type) && enough_time_elapsed(last_clone_time, type, 10)) {
            set_related_type_times(type, last_clone_time);
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

static struct timespec last_unclone_time[N_CLONEABLE_TYPES];

int unclone_type(struct dfg_msu_type *type) {
    struct dfg_msu *to_remove = type->instances[type->n_instances-1];
    int rtn = unclone_msu(to_remove->id);
    if (rtn < 0) {
        log(LOG_SCHEDULING, "Uncloning MSU failed");
        return -1;
    }
    set_haproxy_weights();
    log(LOG_SCHEDULING_DECISIONS, "UNCLONING TYPE %d", type->id);
    return 0;
}

bool can_unclone_latest(struct dfg_msu_type *type) {
    struct dfg_msu *last_msu = type->instances[type->n_instances - 1];
    struct dfg_runtime *rt = last_msu->scheduling.runtime;

    for (int i=0; i < type->n_dependencies; i++) {
        struct dfg_msu_type* dep_type = type->dependencies[i]->type;
        int n_of_type = 0;
        for (int j=0; j < dep_type->n_instances; j++) {
            if (dep_type->instances[j]->scheduling.runtime == rt) {
                n_of_type += 1;
            }
        }
        if (n_of_type > 1) {
            return false;
        }
    }
    return true;
}

int try_to_unclone_type(struct dfg_msu_type *type) {
    if (min_msu_types[type->id] >= type->n_instances) {
        return 0;
    }

    if (reaching_runtime_limit(type, true)) {
        set_related_type_times(type, last_try_clone_time);
        return 0;
    }

    struct dfg_msu *last_msu = type->instances[type->n_instances - 1];
    double eq_0 = p_qlen_max_eq_0(type, last_msu->scheduling.runtime);

    if (eq_0 > 0) {
        if (enough_time_elapsed(last_unclone_time, type, 50) &&
                enough_time_elapsed(last_try_clone_time, type, 200) &&
                can_unclone_latest(type)) {
            log(LOG_SCHEDULING_DECISIONS, "eq0 is %f", eq_0);
            set_related_type_times(type, last_unclone_time);
            unclone_type(type);
        }
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
