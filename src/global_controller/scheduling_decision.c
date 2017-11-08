#include "controller_dfg.h"
#include "stats.h"
#include "msu_stats.h"
#include "msu_ids.h"
#include "logging.h"
#include "haproxy.h"
#include "scheduling.h"

#include <stdbool.h>

#define MAX_CLONE_CONDITIONS 2

struct cloning_info {
    int msu_type_id;
    enum stat_id stat_id;
    double threshold;
    int n_samples;
    double stats[MAX_MSU];
    int num_msus;
    struct timespec last;
};

struct clone_decision {
    int clone_type_id;
    struct cloning_info conditions[MAX_CLONE_CONDITIONS];
    int num_conditions;
    int min_instances;
    struct timespec last;
};

#define CLONING_SAMPLES 10
static struct clone_decision CLONING_DECISIONS[] = {
    { WEBSERVER_READ_MSU_TYPE_ID, {
    { WEBSERVER_READ_MSU_TYPE_ID, MSU_QUEUE_LEN, 2, CLONING_SAMPLES}}, 1},
    { WEBSERVER_REGEX_MSU_TYPE_ID, {
    { WEBSERVER_REGEX_MSU_TYPE_ID, MSU_QUEUE_LEN, 1, CLONING_SAMPLES}}, 1},
    { WEBSERVER_READ_MSU_TYPE_ID, {
    { WEBSERVER_HTTP_MSU_TYPE_ID, MSU_NUM_STATES, 820, CLONING_SAMPLES}}, 1}
};

#define UNCLONING_SAMPLES 50
static struct clone_decision UNCLONING_DECISIONS[] = {
    { WEBSERVER_READ_MSU_TYPE_ID, {
    { WEBSERVER_READ_MSU_TYPE_ID, MSU_QUEUE_LEN, .01, UNCLONING_SAMPLES},
    { WEBSERVER_HTTP_MSU_TYPE_ID, MSU_NUM_STATES, 410, UNCLONING_SAMPLES}}, 2},
    { WEBSERVER_REGEX_MSU_TYPE_ID, {
    { WEBSERVER_REGEX_MSU_TYPE_ID, MSU_QUEUE_LEN, .01, UNCLONING_SAMPLES}}, 1}
};


#define CLONING_DECISION_LEN sizeof(CLONING_DECISIONS) / sizeof(*CLONING_DECISIONS)
#define UNCLONING_DECISION_LEN sizeof(UNCLONING_DECISIONS) / sizeof(*UNCLONING_DECISIONS)

static int gather_cloning_info(struct cloning_info *info) {

    info->num_msus = 0;

    struct dfg_msu_type *type = get_dfg_msu_type(info->msu_type_id);
    if (type == NULL) {
        return 0;
    }

    for (int i=0; i<type->n_instances; i++) {
        struct dfg_msu *instance = type->instances[i];
        struct timed_rrdb *stat = get_stat(info->stat_id, instance->id);

        if (stat == NULL) {
            continue;
        }

        info->stats[info->num_msus] = average_n(stat, info->n_samples);
        if (info->stats[info->num_msus] < 0) {
            continue;
        }
        info->num_msus++;
    }
    return 0;
}

static int gather_cloning_decision(struct clone_decision *decision) {
    for (int i=0; i < decision->num_conditions; i++) {
        gather_cloning_info(&decision->conditions[i]);
    }
    return 0;
}

static bool should_clone(struct clone_decision *decision) {
    for (int i=0; i<decision->num_conditions; i++) {
        struct cloning_info *info = &decision->conditions[i];
        double sum = 0;
        for (int i=0; i<info->num_msus; i++) {
            sum += info->stats[i];
        }

        double mean = sum / info->num_msus;
        bool do_clone =  mean > info->threshold;
        if (do_clone) {
            log(LOG_SCHEDULING_DECISIONS, "Trying to clone type %d due to mean: %.2f",
                info->msu_type_id,  mean);
            return true;
        }
    }
    return false;
}

static bool should_unclone(struct clone_decision *decision) {
    struct dfg_msu_type *clone_type = get_dfg_msu_type(decision->clone_type_id);
    if (clone_type == NULL) {
        return false;
    }
    if (clone_type->n_instances <= decision->min_instances) {
        return false;
    }
    struct cloning_info *info;
    double max = 0;
    for (int i=0; i < decision->num_conditions; i++) {
        info = &decision->conditions[i];
        if (info->num_msus == 0) {
            return false;
        }
        max = 0;
        for (int i=0; i<info->num_msus; i++) {
            if (info->stats[i] > info->threshold) {
                return false;
            }
            max = max > info->stats[i] ? max : info->stats[i];
        }
    }
    log(LOG_SCHEDULING_DECISIONS, "Trying to unclone type %d due to max : %.2f",
        decision->clone_type_id, max);
    return true;
}

#define MIN_CLONE_DURATION_MS 750
#define MIN_UNCLONE_DURATION_MS 750

int try_to_clone() {
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);
    for (int i=0; i<CLONING_DECISION_LEN; i++) {
        struct clone_decision *decision = &CLONING_DECISIONS[i];
        if (((cur_time.tv_sec - decision->last.tv_sec) * 1000 ) + ((cur_time.tv_nsec - decision->last.tv_nsec) * 1e-6) > MIN_CLONE_DURATION_MS) {
            int rtn = gather_cloning_decision(decision);
            if (rtn < 0) {
                log_error("Error getting cloning decision %d", i);
                continue;
            }

            if (should_clone(decision)) {
                decision->last = cur_time;
                struct dfg_msu_type *clone_type = get_dfg_msu_type(decision->clone_type_id);
                struct dfg_msu *cloned_msu = clone_type->instances[0];
                struct dfg_msu *msu = clone_msu(cloned_msu->id);
                if (msu == NULL) {
                    log_error("Cloning MSU failed for MSU type %d",
                              decision->clone_type_id);
                    continue;
                }
                set_haproxy_weights(0,0);
            }
        }
    }
    return 0;
}

int try_to_unclone() {
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);
    for (int i=0; i<UNCLONING_DECISION_LEN; i++) {
        struct clone_decision *decision = &UNCLONING_DECISIONS[i];
        if (((cur_time.tv_sec - decision->last.tv_sec) * 1000 ) + ((cur_time.tv_nsec - decision->last.tv_nsec) * 1e-6) > MIN_UNCLONE_DURATION_MS) {
            int rtn = gather_cloning_decision(decision);
            if (rtn < 0) {
                log_error("Error getting cloning info %d", i);
                continue;
            }
            if (should_unclone(decision)) {
                decision->last = cur_time;
                struct dfg_msu_type *clone_type = get_dfg_msu_type(decision->clone_type_id);
                struct dfg_msu *cloned_msu = clone_type->instances[clone_type->n_instances-1];
                int rtn = unclone_msu(cloned_msu->id);
                if (rtn < 0) {
                    log(LOG_SCHEDULING, "Uncloning MSU failed for MSU type %d",
                        decision->clone_type_id);
                    continue;
                }
                set_haproxy_weights(0,0);
            }
        }
    }
    return 0;
}

static bool min_instances_recorded = false;

int perform_cloning() {
    if (!min_instances_recorded) {
        for (int i=0; i < UNCLONING_DECISION_LEN; i++) {
            struct dfg_msu_type *clone_type = get_dfg_msu_type(UNCLONING_DECISIONS[i].clone_type_id);
            if (clone_type != NULL) {
                UNCLONING_DECISIONS[i].min_instances = clone_type->n_instances;
            }
        }
        min_instances_recorded = true;
    }
    try_to_clone();
    try_to_unclone();
    return 0;
}
