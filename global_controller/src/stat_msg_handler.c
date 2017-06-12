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
            index = msu->statistics->data_queue_size->timepoint;

            // Manage queue length
            if (stats->data_queue_size > average(msu, QUEUE_LEN) * 2) {
                debug("reported queue length for msu %d is twice the current average",
                      stats[i].msu_id);
            }
        }
    }


    struct dfg_config *dfg_config_g = NULL;
    dfg_config_g = get_dfg();

    if (dfg_config_g == NULL) {
        debug("ERROR: %s", "could not retrieve dfg");
        return;
    }

    //Maybe check for that condition multiple times (to ensure that it is a permanent/real behaviour)
    //Also check when the last action was triggered
    // Manage queue length
    short timepoint = msu->satistics->queue_length->timepoint;
    msu->statistics->queue_length->data[timepoint] = stats->data_queue_size;
    timepoint++;
    if (stats->data_queue_size > average(msu, QUEUE_LEN) * 2) {
        char data[32];
        memset(data, '\0', sizeof(data));
        struct dfg_vertex *new_msu = (struct dfg_vertex *) malloc (sizeof(struct dfg_vertex));
        //TODO: check whether this MSU is legit
        pthread_mutex_lock(dfg_config_g->dfg_mutex);
        struct dfg_vertex *msu = dfg_config_g->vertices[stats->msu_id - 1];

        int target_runtime_sock, target_thread_id, ret;
        ret = find_placement(&target_runtime_sock, &target_thread_id);

        if (ret == -1) {
            debug("ERROR: %s", "could not find runtime or thread to place new clone");
            return;
        }

        // Check if we need to spawn a new worker thread
        //right now we assume the new thread doesn't exist.
        create_worker_thread(target_runtime_sock);
        sleep(2);


        // Deep copy of msu to be cloned and update specific fields
        memcpy(new_msu, msu, sizeof(*msu));

        new_msu->msu_id = dfg_config_g->vertex_cnt + 1;
        new_msu->thread_id = target_thread_id;
        // clone routes counts will be incremented when actually created
        new_msu->num_dst = 0;
        new_msu->num_src = 0;
        // clean up target and destination pointers
        int r;
        for (r = 0; r < msu->num_dst; r++) {
            new_msu->msu_dst_list[r] = NULL;
        }
        for (r = 0; r < msu->num_src; r++) {
            new_msu->msu_src_list[r] = NULL;
        }

        pthread_mutex_unlock(dfg_config_g->dfg_mutex);

        snprintf(data, strlen(new_msu->msu_mode) + 1 + how_many_digits(new_msu->thread_id) + 1,
                 "%s %d", new_msu->msu_mode, new_msu->thread_id);

        add_msu(&data, new_msu, target_runtime_sock);

        // Then update downstream and upstream routes
        int action;
        action = MSU_ROUTE_ADD;

        //Add route to downstream
        //TODO: see todo in dfg.c (handle the holes in the list due to MSU_ROUTE_DEL)
        debug("DEBUG: %s", "updating routes for newly cloned msu");
        struct dedos_control_msg update_route_downstream;
        char cmd[128];
        int len, base_len;
        //TODO: determine locality on a per destination basis
        int locality = 1;

        // 1 char for '\0', 5 for spaces
        base_len = 1 + 5;
        base_len = base_len + how_many_digits(new_msu->msu_id);
        base_len = base_len + how_many_digits(new_msu->msu_type);

        int dst;
        for (dst = 0; dst < msu->num_dst; dst++) {
            memset(cmd, '\0', sizeof(cmd));
            len = base_len;
            len = len + how_many_digits(new_msu->msu_runtime->sock);
            len = len + how_many_digits(msu->msu_dst_list[dst]->msu_id);
            len = len + how_many_digits(msu->msu_dst_list[dst]->msu_type);
            len = len + how_many_digits(locality);

            snprintf(cmd, len, "%d %d %d %d %d %d",
                     new_msu->msu_runtime->sock,
                     new_msu->msu_id,
                     new_msu->msu_type,
                     msu->msu_dst_list[dst]->msu_id,
                     msu->msu_dst_list[dst]->msu_type,
                     locality);

            sleep(2);
            send_route_update(cmd, action);
        }

        int src;
        for (src = 0; src < msu->num_src; src++) {
            memset(cmd, '\0', sizeof(cmd));
            len = base_len;
            len = len + how_many_digits(msu->msu_src_list[src]->msu_runtime->sock);
            len = len + how_many_digits(msu->msu_src_list[src]->msu_id);
            len = len + how_many_digits(msu->msu_src_list[src]->msu_type);
            len = len + how_many_digits(locality);

            snprintf(cmd, len, "%d %d %d %d %d %d",
                     msu->msu_src_list[src]->msu_runtime->sock,
                     msu->msu_src_list[src]->msu_id,
                     msu->msu_src_list[src]->msu_type,
                     new_msu->msu_id,
                     new_msu->msu_type,
                     locality);

            sleep(2);
            send_route_update(cmd, action);
        }
    }
}
*/
