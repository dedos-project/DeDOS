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
    struct dfg_msu *sample_msu;
    double stats[MAX_MSU];
    int num_msus;
};

static struct cloning_info CLONING_INFO[] = {
    { WEBSERVER_READ_MSU_TYPE_ID, MSU_QUEUE_LEN, 20 },
    { WEBSERVER_REGEX_MSU_TYPE_ID, MSU_QUEUE_LEN, 20 },
    { WEBSERVER_HTTP_MSU_TYPE_ID, MSU_NUM_STATES, 2048 }
};

#define SAMPLES_FOR_CLONING_DECISION 20

#define CLONING_INFO_LEN sizeof(CLONING_INFO) / sizeof(*CLONING_INFO)

static int gather_cloning_info(struct cloning_info *info) {

    info->num_msus = 0;

    struct dfg_msu_type *type = get_dfg_msu_type(info->msu_type_id);
    if (!type->cloneable) {
        return 0;
    }

    log(LOG_SCHEDULING_DECISIONS, "N instances: %d", type->n_instances);
    for (int i=0; i<type->n_instances; i++) {
        struct dfg_msu *instance = type->instances[i];
        struct timed_rrdb *stat = get_stat(info->stat_id, instance->id);

        if (stat == NULL) {
            log_error("Cannot get stat sample for cloning decision for stat %d.%u",
                      type->id, instance->id);
            continue;
        }

        info->stats[info->num_msus] = average_n(stat, SAMPLES_FOR_CLONING_DECISION);
        if (info->stats[info->num_msus] < 0) {
            log_error("Error averaging statistics");
            continue;
        }
        info->num_msus++;
        info->sample_msu = instance;
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
        log(LOG_SCHEDULING_DECISIONS, "Trying to clone");
    } else {
        log(LOG_SCHEDULING_DECISIONS, "Not trying to clone (mean: %f)", mean);
    }
    return do_clone;
}

static void set_haproxy_weights() {
    int n_reads[MAX_RUNTIMES+1];
    memset(n_reads, 0, sizeof(n_reads));
    struct dfg_msu_type *type = get_dfg_msu_type(WEBSERVER_READ_MSU_TYPE_ID);
    if (type == NULL) {
        log_error("Error getting read type");
        return;
    }
    for (int i=0; i<type->n_instances; i++) {
        int rt_id = type->instances[i]->scheduling.runtime->id;
        n_reads[rt_id]++;
    }

    for (int i=0; i <= MAX_RUNTIMES; i++) {
        if (n_reads[i] > 0) {
            reweight_haproxy(i, n_reads[i]);
        }
    }
}

#define MIN_CLONE_DURATION 2

static struct timespec last_clone_time;

int perform_cloning() {
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);
    if (cur_time.tv_sec - last_clone_time.tv_sec > MIN_CLONE_DURATION) {
        log(LOG_SCHEDULING_DECISIONS, "Gathering cloning info");
        for (int i=0; i<CLONING_INFO_LEN; i++) {
            int rtn = gather_cloning_info(&CLONING_INFO[i]);
            if (rtn < 0) {
                log_error("Error getting cloning info %d", i);
                continue;
            }
            if (should_clone(&CLONING_INFO[i])) {
                struct dfg_msu *msu = clone_msu(CLONING_INFO[i].sample_msu->id);
                if (msu == NULL) {
                    log_error("Cloning MSU failed for MSU type %d", CLONING_INFO[i].msu_type_id);
                    continue;
                }
                clock_gettime(CLOCK_MONOTONIC, &last_clone_time);
                set_haproxy_weights();
                break;
            }
        }
    } else {
        log(LOG_SCHEDULING_DECISIONS, "Too soon to clone");
    }
    return 0;
}
