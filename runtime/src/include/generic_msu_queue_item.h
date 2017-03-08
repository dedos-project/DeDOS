#ifndef GENERIC_MSU_QUEUE_ITEM_H_
#define GENERIC_MSU_QUEUE_ITEM_H_
#include "logging.h"

/* definition of struct that is an item of generic msu queue */
typedef struct generic_msu_queue_item{

    struct generic_msu_queue_item *next;
    /*len of actual item data and pointer to the buffer
     This helps in hiding the structure of data for different MSU's from runtime
     the msus can parse the buffer into struct which they expect it input data
     to be in
     */
    uint32_t buffer_len;
    void *buffer;
} msu_queue_item_t;

static inline struct generic_msu_queue_item * create_generic_msu_queue_item(){
    struct generic_msu_queue_item *item;
    item = (struct generic_msu_queue_item *)malloc(sizeof(struct generic_msu_queue_item));
    if(!item){
        log_error("Unable to malloc generic_msu_queue_item %s","");
        return NULL;
    }
    item->buffer_len = 0;
    item->buffer = NULL;
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
