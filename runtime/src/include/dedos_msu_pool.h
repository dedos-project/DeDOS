#ifndef DEDOS_MSU_POOL_H_
#define DEDOS_MSU_POOL_H_

#include "dedos_threads.h"
#include "control_protocol.h"
#include "generic_msu.h"
#include "logging.h"

/* single linked list to keep track of MSU assigned to each thread */
/* can be changed later depending on the scheduling algorithm FUTURE WORK */
#define DEBUG_MSU_POOL 1

struct msu_pool {
    int num_msus;
    struct generic_msu *head;
    struct generic_msu *tail;
    pthread_mutex_t *mutex;
};

#define MUTEX_LOCK(x) { \
        if (x == NULL) \
            x = mutex_init(); \
        mutex_lock(x); \
}
#define MUTEX_UNLOCK(x) mutex_unlock(x)
#define MUTEX_DEL(x) mutex_deinit(x)

static inline void dedos_msu_pool_print(struct msu_pool *q)
{
    struct generic_msu *cur;
    MUTEX_LOCK(q->mutex);
    cur = q->head;
    if(!q->head){
        log_debug("Empty msu pool %s","");
        MUTEX_UNLOCK(q->mutex);
        return;
    }
    log_info("Total MSUs in pool: %d",q->num_msus);
    do {
        log_debug("MSU_NAME: %s",cur->type->name);
        log_debug("MSU_ID: %d",cur->id);
        log_debug("Proto_number: %u",cur->type->proto_number);
        cur = cur->next;

    } while (cur);
    MUTEX_UNLOCK(q->mutex);
}

static inline int32_t dedos_msu_pool_add(struct msu_pool *q, struct generic_msu *p)
{
    if(q == NULL){
        log_error("%s","NULL msu_pool pointer..cannot add msu to pool");
        return -1;
    }
    if(p == NULL){
        log_error("%s","NULL msu pointer..cannot add null pointer to msu_pool");
        return -1;
    }
    p->next = NULL;

    MUTEX_LOCK(q->mutex);
    if (!q->head) {
        q->head = p;
        q->tail = p;
        q->num_msus = 0;
    } else {
        q->tail->next = p;
        q->tail = p;
    }
    q->num_msus++;
    MUTEX_UNLOCK(q->mutex);

#ifdef DEBUG_MSU_POOL
    dedos_msu_pool_print(q);
#endif
    return 0;
}

static inline struct generic_msu* dedos_msu_pool_find(struct msu_pool *q, int id){
    if(!q->head){
        return NULL;
    }
    struct generic_msu *cur;

    MUTEX_LOCK(q->mutex);
    cur = q->head;
    do {
        if(cur->id == id){
        	MUTEX_UNLOCK(q->mutex);
            return cur;
        }
        cur = cur->next;
    } while(cur);

    MUTEX_UNLOCK(q->mutex);
    return NULL;
}

static inline void dedos_msu_pool_delete(struct msu_pool *q, struct generic_msu *m)
{
    if(!q->head){
        return;
    }
    struct generic_msu *cur, *prev;
    int del = 0;

    MUTEX_LOCK(q->mutex);
    cur = q->head;
    do {
        if(cur->id == m->id){
            if(cur == q->head){ //head node
                q->head = cur->next;
                del = 1;
                break;
            } else if(cur == q->tail){ //end node
                q->tail = prev;
                prev->next = NULL;
                del = 1;
                break;
            } else { //middle
                prev->next = cur->next;
                del = 1;
                break;
            }
        }
        prev = cur;
        cur = cur->next;
    } while(cur);

    if(del == 1){
        q->num_msus--;
    }

    MUTEX_UNLOCK(q->mutex);

#ifdef DEBUG_MSU_POOL
    dedos_msu_pool_print(q);
#endif
}

static inline void empty_generic_msu_list(struct msu_pool *q, struct generic_msu* msu){
    if(msu == NULL){
        log_debug("msu == NULL%s","");
        return;
    }
    empty_generic_msu_list(q, msu->next);
    destroy_msu(msu);
    q->num_msus--;
    if(q->num_msus == 0){
        q->head = NULL;
        q->tail = NULL;
    }
}

static inline void dedos_msu_pool_destroy(struct msu_pool* msu_pool){

    struct generic_msu *cur;

    MUTEX_LOCK(msu_pool->mutex);
    if(!msu_pool->head){
        log_debug("Empty msu pool %s","");
        MUTEX_UNLOCK(msu_pool->mutex);
        return;
    }
    cur = msu_pool->head;
    log_debug("Emptying msu_pool list %s","");
    //RESUME HERE
    empty_generic_msu_list(msu_pool, cur);
    if(msu_pool->num_msus != 0){
        log_error("Failed to empty msu_pool %s","");
        return;
    }
    MUTEX_UNLOCK(msu_pool->mutex);
#ifdef DEBUG_MSU_POOL
    dedos_msu_pool_print(msu_pool);
#endif
}

#endif
