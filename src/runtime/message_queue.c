#include "message_queue.h"
#include "logging.h"

int enqueue_msg(struct msg_queue *q, struct dedos_msg *msg) {
    if (q->shared) {
        if (pthread_mutex_lock(&q->mutex) != 0) {
            log_error("Error locking msg queue mutex");
            return -1;
        }
    }

    msg->next = NULL;
    if (!q->head) {
        q->head = msg;
        q->tail = msg;
        q->num_msgs = 1;
    } else {
        q->tail->next = msg;
        q->tail = msg;
        q->num_msgs++;
    }

    if (q->shared) {
        if (pthread_mutex_unlock(&q->mutex) != 0) {
            log_error("Error unlocking msg queue mutex");
            return -1;
        }
    }

    if (q->sem) {
        sem_post(q->sem);
    }

    return 0;
}


struct dedos_msg *dequeue_msg(struct msg_queue *q) {
    if (q->shared) {
        pthread_mutex_lock(&q->mutex);
    }

    struct dedos_msg *msg = q->head;
    if (msg == NULL) {
        if (q->shared) {
            pthread_mutex_unlock(&q->mutex);
        }
        log_custom(LOG_FAILED_DEQUEUE_ATTEMPTS, 
                   "Attempted to dequeue NULL message");
        return NULL;
    }

    q->head = msg->next;
    q->num_msgs--;
    if (q->head == NULL) {
        q->tail = NULL;
    }

    msg->next = NULL;

    if (q->shared) {
        pthread_mutex_unlock(&q->mutex);
    }

    return msg;
}

int init_msg_queue(struct msg_queue *q, sem_t *sem) {
    q->num_msgs = 0;
    q->head = NULL;
    q->tail = NULL;
    pthread_mutex_init(&q->mutex, NULL);
    q->sem = sem;
    q->shared = true;
    return 0;
}
