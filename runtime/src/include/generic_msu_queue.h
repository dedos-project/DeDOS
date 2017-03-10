#ifndef GENERIC_MSU_QUEUE_H_
#define GENERIC_MSU_QUEUE_H_
#include "control_protocol.h"
#include "generic_msu_queue_item.h"
#include "dedos_threads.h"
#include "logging.h"

#define Q_LIMIT 0

#define MAX_MSU_Q_SIZE 1024 * 1024 * 100
#define MAX_MSU_Q_MSGS 100000 * 100

#ifndef NULL
#define NULL ((void *)0)
#endif


typedef struct generic_msu_queue {
    uint32_t num_msgs;
    uint32_t size;
    uint32_t max_msgs;
    uint32_t max_size;
    struct generic_msu_queue_item *head;
    struct generic_msu_queue_item *tail;
    void *mutex;
    uint8_t shared;
    uint16_t overhead;
} msu_queue;


static inline void generic_msu_queue_init(struct generic_msu_queue *q){
    q->num_msgs = 0;
    q->size = 0;
    q->max_msgs = MAX_MSU_Q_MSGS;
    q->max_size = MAX_MSU_Q_SIZE;
    q->head = NULL;
    q->tail = NULL;
    q->overhead = 0;
}

static inline int32_t generic_msu_queue_enqueue(struct generic_msu_queue *q, struct generic_msu_queue_item *p)
{
    int retsize = 0;
    if (q->shared)
        mutex_lock(q->mutex);

    if ((q->max_msgs) && (q->max_msgs <= q->num_msgs)){
        log_error("Failure max_msgs = %u, num_msgs = %u",q->max_msgs, q->num_msgs);

        if (q->shared)
            mutex_unlock(q->mutex);

        return -1;
    }
    if ((q->max_size) && (q->max_size < (p->buffer_len + q->size))){
        log_error("Failure max_size = %u, after enqueuing q_size = %u",q->max_size, p->buffer_len + q->size);
        if (q->shared)
            mutex_unlock(q->mutex);

        return -1;
    }

    p->next = NULL;
    if (!q->head) {
        q->head = p;
        q->tail = p;
        q->size = 0;
        q->num_msgs = 0;
    } else {
        q->tail->next = p;
        q->tail = p;
    }

    q->size += p->buffer_len;
    retsize = q->size;
    q->num_msgs++;

    if (q->shared)
        mutex_unlock(q->mutex);

    return (int32_t)retsize;
}

static inline struct generic_msu_queue_item *generic_msu_queue_dequeue(struct generic_msu_queue *q)
{

    if (q->shared)
        mutex_lock(q->mutex);

    struct generic_msu_queue_item *p = q->head;
    if (!p){
        if (q->shared)
            mutex_unlock(q->mutex);
        return NULL;
    }

    if (q->num_msgs < 1){
        if (q->shared)
            mutex_unlock(q->mutex);

        return NULL;
    }

    q->head = p->next;
    q->num_msgs--;
    q->size -= p->buffer_len - q->overhead;
    if (q->head == NULL)
        q->tail = NULL;

    p->next = NULL;
    if (q->shared)
        mutex_unlock(q->mutex);

    return p;
}

static inline struct generic_msu_queue_item *generic_msu_queue_peek(struct generic_msu_queue *q)
{
    struct generic_msu_queue_item *p = q->head;
    if (q->num_msgs < 1)
        return NULL;

    return p;
}

static inline void generic_msu_queue_deinit(struct generic_msu_queue *q)
{
    if (q->shared) {
        mutex_deinit(q->mutex);
    }
}

static inline void generic_msu_queue_empty(struct generic_msu_queue *q)
{
    struct generic_msu_queue_item *p = generic_msu_queue_dequeue(q);
    while(p) {
        delete_generic_msu_queue_item(p);
        p = generic_msu_queue_dequeue(q);
    }
}

static inline void generic_msu_queue_protect(struct generic_msu_queue *q)
{
    q->shared = 1;
}

static inline int32_t generic_msu_queue_enqueue_at_head (struct generic_msu_queue *q, struct generic_msu_queue_item *p)
{
    int retsize = 0;
    if (q->shared)
        mutex_lock(q->mutex);

    if ((q->max_msgs) && (q->max_msgs <= q->num_msgs)){
        log_error("Failure max_msgs = %u, num_msgs = %u",q->max_msgs, q->num_msgs);

        if (q->shared)
            mutex_unlock(q->mutex);

        return -1;
    }
    if ((q->max_size) && (q->max_size < (p->buffer_len + q->size))){
        log_error("Failure max_size = %u, after enqueuing q_size = %u",q->max_size, p->buffer_len + q->size);
        if (q->shared)
            mutex_unlock(q->mutex);

        return -1;
    }

    p->next = q->head;
    if (q->head == NULL) {
        q->tail = p;
    }
    q->head = p;
    
    q->size += p->buffer_len;
    retsize = q->size;
    q->num_msgs++;

    if (q->shared)
        mutex_unlock(q->mutex);

    return (int32_t)retsize;
}

#endif
