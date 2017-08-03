/**
 * msu_queue.c
 *
 * Will contain functions relevant to the enqueing and dequeing of data
 * from an MSU queue.
 * For the moment, some functionality may be in `generic_msu_queue.h` instead.
 */

#include "generic_msu_queue.h"
#include "generic_msu.h"
#include "communication.h" // for runtimeIpAddress

#ifndef LOG_INITIAL_ENQUEUES
#define LOG_INITIAL_ENQUEUES 0
#endif

void add_to_msu_path(struct generic_msu_queue_item *queue_item,
                     int type_id, int id, uint32_t ip_address) {
    struct msu_path_element *path = &queue_item->path[queue_item->path_index];
    path->type_id = type_id;
    path->msu_id = id;
    path->ip_address = ip_address;
    queue_item->path_index++;
    queue_item->path_index %= MAX_PATH_LEN;
}

struct msu_path_element *get_path_history(struct generic_msu_queue_item *queue_item, 
                                          int reverse_index){
    int index = queue_item->path_index - reverse_index;
    if (index < 0) {
        index = MAX_PATH_LEN + index;
    }
    if (index < 0) {
        log_error("Cannot look that far back in msu queue history");
        return NULL;
    }
    return &queue_item->path[index];
}

/**
 * Creates a new MSU queue item and enqueues it on the appropriate MSU.
 * This function is meant to be used only for the introduction of a queue item
 * into the stack, and not for subsequently passing queue items between MSUs
 * (which should just use the return of the msu_receive function)
 */
int initial_msu_enqueue(void *buffer, size_t buffer_len, uint32_t id, struct generic_msu *dest) {
    struct generic_msu_queue_item *queue_item = create_generic_msu_queue_item();

    if (queue_item == NULL) {
        log_error("Failed to create queue item");
        return -1;
    }

#ifdef DATAPLANE_PROFILING
    queue_item->dp_profile_info.dp_id = get_request_id();
    log_dp_event(-1, DEDOS_ENTRY, &new_item_ws->dp_profile_info);
#endif

    add_to_msu_path(queue_item, dest->type->type_id, dest->id, runtimeIpAddress);
    int q_len = generic_msu_queue_enqueue(&dest->q_in, queue_item);

    if ( q_len < 0 ) {
        log_error("Failed to perform intial enqueing of request %d to %s (%d)",
                  id, dest->type->name, dest->id);
        delete_generic_msu_queue_item(queue_item);
        return -1;
    } else {
        log_custom(LOG_INITIAL_ENQUEUES, "Introduced queue item %d at MSU %s (%d)",
                   id, dest->type->name, dest->id);
    }

    return 0;
}
