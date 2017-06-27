#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "tmp_msu_headers.h"
#include "timeseries.h"
#include "controller_tools.h"
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

#if DYN_SCHED
    for (int i = 0; i < stats->n_samples; i++) {
        struct stat_sample *sample = &stats->samples[i];

        // Will have to move once we get non-msu stats
        struct dfg_vertex *msu = get_msu_from_id(sample->item_id);

        if (msu == NULL) {
            log_error("Could not get MSU with ID %d", sample->item_id);
            return -1;
        }


        int ret;
        switch (sample->stat_id) {
            case QUEUE_LEN:
                //There is an odious situation bellow. Can you figure it out?
                if (msu->msu_type == DEDOS_SSL_READ_MSU_ID) {
                    if (average(get_msu_from_id(sample->item_id), sample->stat_id) > 0) {
                        struct msus_of_type *http_dep = get_msus_from_type(DEDOS_WEBSERVER_MSU_ID);

                        errored = clone_msu(http_dep->msu_ids[0]);
                    }
                }
/*
                if (average(get_msu_from_id(sample->item_id), sample->stat_id) > 5) {
                    clone_msu(7);
                }
*/
/*
                if (msu->msu_type == DEDOS_SSL_READ_MSU_ID) {
                    struct msus_of_type *sslreads = get_msus_from_type(500);
                    //Assume fixed initial number of MSUs on rt1
                    int n_msu = sslreads->num_msus - 6;

                    if (sample->stats->stat > 5) {
                        ret = clone_msu(msu->msu_id);
                        sleep(5);
                        if (ret != -1) {
                            const char cmd[64];
                            snprintf(cmd, 64, "echo 'set weight https/s2 %d' | socat stdio /tmp/haproxy.socket", n_msu + 1);
                            system(cmd);
                        }
                    }
                }
*/
                break;

            default:
                //log_debug("No action implemented for stat_id %d", sample->stat_id);
                continue;
        }
    }
#endif

    return errored;
}
