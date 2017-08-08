#ifndef GENERIC_MSU_QUEUE_ITEM_H_
#define GENERIC_MSU_QUEUE_ITEM_H_
#include "data_plane_profiling.h"
#include "msu_queue.h"
#include <stdlib.h>
#include <stdint.h>
#include "logging.h"

/* definition of struct that is an item of generic msu queue */
struct generic_msu_queue_item {
    struct queue_key key;
    struct generic_msu_queue_item *next;
    struct msu_path_element path[MAX_PATH_LEN];
    int path_index;
    /*len of actual item data and pointer to the buffer
     This helps in hiding the structure of data for different MSU's from runtime
     the msus can parse the buffer into struct which they expect it input data
     to be in
     */
    uint32_t buffer_len;
    void *buffer;
#ifdef DATAPLANE_PROFILING
    struct dataplane_profile_info dp_profile_info;
#endif
};

static inline struct generic_msu_queue_item * create_generic_msu_queue_item(){
    struct generic_msu_queue_item *item;
    item = (struct generic_msu_queue_item *)malloc(sizeof(struct generic_msu_queue_item));
    if(!item){
        log_error("Unable to malloc generic_msu_queue_item %s","");
        return NULL;
    }
    item->path_index = 0;
    memset(&item->key, 0, sizeof(item->key));
    item->buffer_len = 0;
    item->buffer = NULL;
#ifdef DATAPLANE_PROFILING
    memset(&item->dp_profile_info.dp_log_entries, 0, sizeof(item->dp_profile_info.dp_log_entries));
    memset(&item->dp_profile_info, 0, sizeof(struct dataplane_profile_info));
#endif
    return item;
}

static inline void delete_generic_msu_queue_item(struct generic_msu_queue_item *item){
    if(!item){
        log_warn("Empty pointer passed to free! %s","");
        return;
    }
//    if(item->buffer_len > 0 && item->buffer!= NULL){
//        free(item->buffer);
//    }
    log_debug("Freeing queue item %s","");
    free(item);
}

#endif /* GENERIC_MSU_QUEUE_ITEM_H_ */
