#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "haproxy.h"
#include "tmp_msu_headers.h"
#include "timeseries.h"
#include "controller_tools.h"
#include "scheduling.h"
#include "stat_msg_handler.h"
#include "communication.h"
#include "control_protocol.h"
#include "dfg.h"
#include "api.h"

#define NEXT_MSU_LOCAL 1
#define NEXT_MSU_REMOTE 2

#ifndef DYN_SCHED
#define DYN_SCHED 1
#endif

#ifndef LOG_PRINT_TIMESERIES
#define LOG_PRINT_TIMESERIES 0
#endif

#ifndef LOG_CLONE_DECISIONS
#define LOG_CLONE_DECISIONS 0
#endif

struct cloning_info {
    int type_id;
    struct dfg_vertex *msu_sample;
    double stat[MAX_MSU];
    int n_msu;
};

struct cloning_info CLONING_INFO[] = {
     { DEDOS_WEBSERVER_READ_MSU_ID },
     { DEDOS_WEBSERVER_REGEX_MSU_ID },
     { DEDOS_WEBSERVER_HTTP_MSU_ID }
};

int CLONING_INFO_LEN = sizeof(CLONING_INFO) / sizeof(struct cloning_info);

int gather_cloning_info(struct stats_control_payload *stats, struct cloning_info *cloning_info) {

    for (int i = 0; i < CLONING_INFO_LEN; i++) {
        cloning_info[i].n_msu = 0;
    }

    struct dfg_config *dfg = get_dfg();

    for (int i = 0; i < dfg->vertex_cnt; i++) {
        struct dfg_vertex *msu = dfg->vertices[i];
        struct cloning_info *type_cloning_info = NULL;
        for (int j = 0; j < CLONING_INFO_LEN; j++) {
            if (cloning_info[j].type_id == msu->msu_type) {
                type_cloning_info = &cloning_info[j];
                break;
            }
        }
        if (type_cloning_info == NULL) {
            //log_warn("Could not aggregate cloning info for %d", msu->msu_id);
            continue;
        }
        type_cloning_info->msu_sample = msu;
        double stat = average_n(msu, N_STATES, STAT_SAMPLE_SIZE * 2);
        if (stat != -1) {
            type_cloning_info->stat[type_cloning_info->n_msu] = stat;
            log_custom(LOG_CLONE_DECISIONS, "Stat of msu %d (%d) is %.2f",
                       i,
                       type_cloning_info->type_id,
                       type_cloning_info->stat[type_cloning_info->n_msu]);
            type_cloning_info->n_msu++;
        } else {
            log_custom(LOG_CLONE_DECISIONS, "Could not get %d stats for msu %d (%d)",
                       STAT_SAMPLE_SIZE, msu->msu_id, type_cloning_info->type_id);
        }

    }
    return CLONING_INFO_LEN;
}

int make_cloning_decision(struct cloning_info *cloning_info, int info_len, struct dfg_vertex **to_clone) {
    double sum_stat[info_len];
    for (int i = 0; i < info_len; i++) {
        sum_stat[i] = 0;
        struct cloning_info *type_info = &cloning_info[i];
        for (int j = 0; j < type_info->n_msu; j++) {
            sum_stat[i] += type_info->stat[j];
            log_custom(LOG_CLONE_DECISIONS, "%d %.2f", j, type_info->stat[j]);
        }
        log_custom(LOG_CLONE_DECISIONS, "Sum of %d stats %d: %2.f", type_info->n_msu, i, sum_stat[i]);
    }

    double min_ratio_stat = sum_stat[0];
    double ratio_stats[info_len];
    for (int i = 0; i < info_len; i++) {
        double ratio_stat = sum_stat[i]; // / (double)cloning_info[i].n_msu;
        if (min_ratio_stat > ratio_stat) {
            min_ratio_stat = ratio_stat;
        }
        ratio_stats[i] = sum_stat[i];// * (double)(cloning_info[i].n_msu) / (double)(cloning_info[i].n_msu + 1);
    }

    log_custom(LOG_CLONE_DECISIONS, "Minimum ratio stat: %.3f", min_ratio_stat);
    int n_to_clone = 0;
    double to_clone_stats[info_len];
    for (int i = 0; i < info_len; i++) {
        log_custom(LOG_CLONE_DECISIONS, "Ratio stat %d (%d): %.3f", i, cloning_info[i].type_id, ratio_stats[i]);

        if (ratio_stats[i] > 2048) {
            // FIXME: THIS MSU IS NOT CLONABLE
            if (cloning_info[i].msu_sample->scheduling.cloneable >= 0) {
                int j;
                for (j=n_to_clone-1; j>=0; j--) {
                    if (to_clone_stats[j] < ratio_stats[i]) {
                        to_clone[j+1] = to_clone[j];
                        to_clone_stats[j+1] = to_clone_stats[j];
                    } else {
                        break;
                    }
                }
                to_clone_stats[j+1] = ratio_stats[i];
                to_clone[j+1] = cloning_info[i].msu_sample;
                n_to_clone++;
            }
        }
    }
    return n_to_clone;
}

void set_haproxy_weights() {
    struct dfg_config *dfg = get_dfg();
    int n_handshakes[MAX_RUNTIMES];
    bzero(n_handshakes, sizeof(n_handshakes));
    int n_runtimes = 0;
    for (int i = 0; i < dfg->vertex_cnt; i++) {
        struct dfg_vertex *msu = dfg->vertices[i];
        if (msu->msu_type == DEDOS_WEBSERVER_READ_MSU_ID) {
            n_handshakes[msu->scheduling.runtime->id]++;
            if (n_runtimes < msu->scheduling.runtime->id)
                n_runtimes = msu->scheduling.runtime->id;
        }
    }
    for (int i = 1; i <= n_runtimes; i++) {
        log_info("Reweighting s%d to %d", i, n_handshakes[i]);
        reweight_haproxy(i, n_handshakes[i]);
    }
}

#define MIN_CLONE_DURATION 2
struct timespec last_clone_time;

//TODO: update this function for new scheduling & DFG structure
int process_stats_msg(struct stats_control_payload *stats, int runtime_sock) {
    int errored = 0;
    for (int i = 0; i < stats->n_samples; i++) {
        struct stat_sample *sample = &stats->samples[i];
        struct timed_rrdb *to_modify;

        // Will have to move once we get non-msu stats
        struct dfg_vertex *msu = get_msu_from_id(sample->item_id);

        if (msu == NULL) {
            log_error("Could not get MSU with ID %d", sample->item_id);
            return -1;
        }

        switch (sample->stat_id) {
            case QUEUE_LEN:
                to_modify = &msu->statistics.queue_length;
                break;
            case ITEMS_PROCESSED:
                to_modify = &msu->statistics.queue_items_processed;
                break;
            case MSU_INTERNAL_TIME:
                to_modify = &msu->statistics.execution_time;
                break;
            case N_STATES:
                to_modify = &msu->statistics.n_states;
                break;
            default:
                log_error("No stat handler for stat_id %d", sample->stat_id);
                errored = -1;
                continue;
        }

        int rtn = append_to_timeseries(sample->stats, sample->n_stats, to_modify);

        if (rtn < 0) {
            errored = -1;
            log_error("Error appending samples to timeseries");
        }

#if LOG_PRINT_TIMESERIES
        log_custom(LOG_PRINT_TIMESERIES, "Timeseries for %d.%d", sample->stat_id, sample->item_id);
        print_timeseries(to_modify);
#endif

    }

    if (errored != 0)
        return errored;

#if DYN_SCHED
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);
    if (cur_time.tv_sec - last_clone_time.tv_sec > MIN_CLONE_DURATION) {
        int info_len = gather_cloning_info(stats, CLONING_INFO);
        struct dfg_vertex *to_clone[info_len];
        int n_to_clone = make_cloning_decision(CLONING_INFO, info_len, to_clone);
        log_custom(LOG_CLONE_DECISIONS, "Cloning %d MSUs", n_to_clone);
        for (int i = 0; i < n_to_clone; i++) {
            if (to_clone[i]->msu_type == DEDOS_WEBSERVER_HTTP_MSU_ID) {
                struct msus_of_type *read_msus = get_msus_from_type(DEDOS_WEBSERVER_READ_MSU_ID);
                if (read_msus != NULL) {
                    to_clone[i] = get_msu_from_id(read_msus->msu_ids[0]);
                    free(read_msus);
                }
            }
            struct dfg_vertex *msu = clone_msu(to_clone[i]->msu_id);
            if (msu == NULL) {
                log_error("Clone msu failed for msu %d", to_clone[i]->msu_id);
                continue;
            } else {
                clock_gettime(CLOCK_MONOTONIC, &last_clone_time);
                set_haproxy_weights();
                break;
            }
        }
    }
    // TODO: Make_removal_decision
#endif
    return 0;
}
