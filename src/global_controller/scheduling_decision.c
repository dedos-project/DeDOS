#include "controller_dfg.h"
#include "stats.h"
#include "msu_stats.h"
#include "msu_ids.h"
#include "logging.h"
#include "haproxy.h"
#include "scheduling.h"

#include <stdbool.h>

struct cloning_info {
    int msu_type_id;
    enum stat_id stat_id;
    double threshold;
    int n_samples;
    int clone_type_id;
    int min_instances;
    double stats[MAX_MSU];
    int num_msus;
};

#define CLONING_SAMPLES 10
static struct cloning_info CLONING_INFO[] = {
    { WEBSERVER_READ_MSU_TYPE_ID, MSU_QUEUE_LEN, 1, CLONING_SAMPLES, WEBSERVER_READ_MSU_TYPE_ID},
    { WEBSERVER_REGEX_MSU_TYPE_ID, MSU_QUEUE_LEN, 1, CLONING_SAMPLES, WEBSERVER_REGEX_MSU_TYPE_ID },
    { WEBSERVER_HTTP_MSU_TYPE_ID, MSU_NUM_STATES, 2048, CLONING_SAMPLES, WEBSERVER_READ_MSU_TYPE_ID }
};

#define UNCLONING_SAMPLES 100
static struct cloning_info UNCLONING_INFO[] = {
    { WEBSERVER_READ_MSU_TYPE_ID, MSU_QUEUE_LEN, .01, UNCLONING_SAMPLES, WEBSERVER_READ_MSU_TYPE_ID, 2},
    { WEBSERVER_REGEX_MSU_TYPE_ID, MSU_QUEUE_LEN, .01, UNCLONING_SAMPLES, WEBSERVER_REGEX_MSU_TYPE_ID, 3},
};


#define CLONING_INFO_LEN sizeof(CLONING_INFO) / sizeof(*CLONING_INFO)
#define UNCLONING_INFO_LEN sizeof(UNCLONING_INFO) / sizeof(*UNCLONING_INFO)

static int gather_cloning_info(struct cloning_info *info) {

    info->num_msus = 0;

    struct dfg_msu_type *type = get_dfg_msu_type(info->msu_type_id);
    if (type == NULL || !type->cloneable) {
        return 0;
    }

    for (int i=0; i<type->n_instances; i++) {
        struct dfg_msu *instance = type->instances[i];
        struct timed_rrdb *stat = get_stat(info->stat_id, instance->id);

        if (stat == NULL) {
            //log_error("Cannot get stat sample for cloning decision for stat %d.%u",
            //          type->id, instance->id);
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

static bool should_clone(struct cloning_info *info) {
    double sum = 0;
    for (int i=0; i<info->num_msus; i++) {
        sum += info->stats[i];
    }

    double mean = sum / info->num_msus;
    bool do_clone =  mean > info->threshold;
    if (do_clone) {
        log(LOG_SCHEDULING_DECISIONS, "Trying to clone due to mean: %.2f", mean);
    }
    return do_clone;
}

static bool should_unclone(struct cloning_info *info) {
    if (info->num_msus == 0) {
        return false;
    }
    double max = 0;
    for (int i=0; i<info->num_msus; i++) {
        if (info->stats[i] > info->threshold) {
            return false;
        }
        max = max > info->stats[i] ? max : info->stats[i];
    }
    if (info->num_msus > info->min_instances) {
        log(LOG_SCHEDULING_DECISIONS, "Trying to unclone due to max : %.2f", max);
        return true;
    }
    return false;
}

#define MIN_CLONE_DURATION 1
#define MIN_UNCLONE_DURATION 2

static struct timespec last_clone_time;
static struct timespec last_unclone_time;

int try_to_clone() {
    for (int i=0; i<CLONING_INFO_LEN; i++) {
        int rtn = gather_cloning_info(&CLONING_INFO[i]);
        if (rtn < 0) {
            log_error("Error getting cloning info %d", i);
            continue;
        }
        if (should_clone(&CLONING_INFO[i])) {
            struct dfg_msu_type *clone_type = get_dfg_msu_type(CLONING_INFO[i].clone_type_id);
            struct dfg_msu *cloned_msu = clone_type->instances[0];
            struct dfg_msu *msu = clone_msu(cloned_msu->id);
            if (msu == NULL) {
                log_error("Cloning MSU failed for MSU type %d", CLONING_INFO[i].msu_type_id);
                continue;
            }
            set_haproxy_weights(0,0);
            return 0;
        }
    }
    return 1;
}

int try_to_unclone() {
    for (int i=0; i<UNCLONING_INFO_LEN; i++) {
        int rtn = gather_cloning_info(&UNCLONING_INFO[i]);
        if (rtn < 0) {
            log_error("Error getting cloning info %d", i);
            continue;
        }
        if (should_unclone(&UNCLONING_INFO[i])) {
            struct dfg_msu_type *clone_type = get_dfg_msu_type(UNCLONING_INFO[i].clone_type_id);
            struct dfg_msu *cloned_msu = clone_type->instances[clone_type->n_instances-1];
            int rtn = unclone_msu(cloned_msu->id);
            if (rtn < 0) {
                log(LOG_SCHEDULING, "Uncloning MSU failed for MSU type %d",
                    CLONING_INFO[i].msu_type_id);
                continue;
            }
            set_haproxy_weights(0,0);
            return 0;
        }
    }
    return 1;
}
int perform_cloning() {
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);
    if (cur_time.tv_sec - last_clone_time.tv_sec >= MIN_CLONE_DURATION) {
        clock_gettime(CLOCK_MONOTONIC, &last_clone_time);
        try_to_clone();
        if (cur_time.tv_sec - last_unclone_time.tv_sec >= MIN_UNCLONE_DURATION) {
            clock_gettime(CLOCK_MONOTONIC, &last_unclone_time);
            try_to_unclone();
        }
    }
    return 0;
}
