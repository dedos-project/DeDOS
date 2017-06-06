#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "timeseries.h"
#include "controller_tools.h"
#include "stat_msg_handler.h"
#include "communication.h"
#include "control_protocol.h"
#include "dfg.h"
#include "api.h"

#define NEXT_MSU_LOCAL 1
#define NEXT_MSU_REMOTE 2

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

        if ( msu == NULL ){
            log_error("Could not get MSU with ID %d", sample->item_id);
            return -1;
        }

        switch ( sample->stat_id ) {
            case QUEUE_LEN:
                to_modify = &msu->statistics.queue_length;
                break;
            case ITEMS_PROCESSED:
                to_modify = &msu->statistics.queue_items_processed;
                break;
            default:
                log_error("No stat handler for stat_id %d", sample->stat_id);
                errored=-1;
                continue;
        }

        int rtn = append_to_timeseries(sample->stats, sample->n_stats, to_modify);

        if (rtn < 0){
            errored=-1;
            log_error("Error appending samples to timeseries");
        }

#if LOG_PRINT_TIMESERIES
        log_custom(LOG_PRINT_TIMESERIES, "Timeseries for %d.%d", sample->stat_id, sample->item_id);
        print_timeseries(to_modify);
#endif

    }


    return errored;
}


/*
    for (i = 0; i <= stats_items; i++) {
        if (stats[i].msu_id > 0) {
            struct dfg_vertex *msu = get_msu_from_id(stats[i].msu_id);
            short index;
            index = msu->statistics.data_queue_size.timepoint;

            // Manage queue length
            if (stats[i].data_queue_size > average(msu, QUEUE_LEN) * 2) {
                debug("reported queue length for msu %d is twice the current average",
                      stats[i].msu_id);
                clone_msu(stats[i].msu_id);
            }
        }
    }
*/
