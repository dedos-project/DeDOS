#ifndef DEDOS_THREAD_QUEUE_H_
#define DEDOS_THREAD_QUEUE_H_
#include "control_protocol.h"
#include "generic_msu_queue.h"

#define Q_LIMIT 0

#define MAX_THREAD_Q_SIZE 4096
#define MAX_THREAD_Q_MSGS 1000

#ifndef NULL
#define NULL ((void *)0)
#endif


struct dedos_thread_queue {
    uint32_t num_msgs;
    uint32_t size;
    uint32_t max_msgs;
    uint32_t max_size;
    struct dedos_thread_msg *head;
    struct dedos_thread_msg *tail;
    pthread_mutex_t *mutex;
    uint8_t shared;
    uint16_t overhead;
};


#ifdef PICO_SUPPORT_DEBUG_TOOLS
static void debug_q(struct dedos_thread_queue *q)
{
    struct dedos_thread_msg *p = q->head;
    dbg("%d: ", q->num_msgs);
    while(p) {
        dbg("(%p)-->", p);
        p = p->next;
    }
    dbg("X\n");
}

#else

#define debug_q(x) do {} while(0)
#endif

static inline int32_t dedos_thread_enqueue(struct dedos_thread_queue *q, struct dedos_thread_msg *p)
{
    int ret_size = 0;

    if (q->shared)
        mutex_lock(q->mutex);

    if ((q->max_msgs) && (q->max_msgs <= q->num_msgs)){

        if (q->shared)
            mutex_unlock(q->mutex);

        return -1;
    }

    if ((q->max_size) && (q->max_size < (p->buffer_len + q->size))){
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
    q->num_msgs++;
    debug_q(q);
    ret_size = q->size;

    if (q->shared)
        mutex_unlock(q->mutex);

    return (int32_t)ret_size;
}

static inline struct dedos_thread_msg *dedos_thread_dequeue(struct dedos_thread_queue *q)
{

    if (q->shared)
        mutex_lock(q->mutex);

    struct dedos_thread_msg *p = q->head;

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
    q->size -= p->buffer_len;
    if (q->head == NULL)
        q->tail = NULL;

    debug_q(q);

    p->next = NULL;
    if (q->shared)
        mutex_unlock(q->mutex);

    return p;
}

static inline struct dedos_thread_msg *dedos_thread_queue_peek(struct dedos_thread_queue *q)
{
    struct dedos_thread_msg *p = q->head;
    if (q->num_msgs < 1)
        return NULL;

    debug_q(q);
    return p;
}

static inline void dedos_thread_queue_deinit(struct dedos_thread_queue *q)
{
    if (q->shared) {
        mutex_deinit(q->mutex);
    }
}

static inline void dedos_thread_queue_empty(struct dedos_thread_queue *q)
{
    struct dedos_thread_msg *p = dedos_thread_dequeue(q);
    while(p) {
        dedos_thread_msg_free(p);
        p = dedos_thread_dequeue(q);
    }
}

static inline void dedos_thread_protect(struct dedos_thread_queue *q)
{
    q->shared = 1;
}

#endif
