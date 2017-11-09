/**
 * @file message_queue.c
 *
 * Structures and functions for enqueueing and dequeuing general-purpose
 * messages from a queue
 */
#include "message_queue.h"
#include "logging.h"

#include <time.h>

/** Interval 0 seconds from now */
static struct timespec zero = {};

int enqueue_msg(struct msg_queue *q, struct dedos_msg *msg) {
    return schedule_msg(q, msg, &zero);
}

int schedule_msg(struct msg_queue *q, struct dedos_msg *msg, struct timespec *interval) {
    if (interval->tv_sec == 0 && interval->tv_nsec == 0) {
        msg->delivery_time = *interval;
    } else {
        clock_gettime(CLOCK_MONOTONIC_COARSE, &msg->delivery_time);
        msg->delivery_time.tv_sec += interval->tv_sec;
        msg->delivery_time.tv_nsec += interval->tv_nsec;
        if (msg->delivery_time.tv_nsec > 1e9) {
            msg->delivery_time.tv_nsec -= 1e9;
            msg->delivery_time.tv_sec += 1;
        }
    }
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

/** Returns the difference in time in seconds, t2 - t1 */
static double timediff_s(struct timespec *t1, struct timespec *t2) {
    return (double)(t2->tv_sec - t1->tv_sec) + (double)(t2->tv_nsec - t1->tv_nsec) * 1e-9;
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
        return NULL;
    }

    int num_checked = 0;
    // Loop until you find a message with a good delivery time
    do {
        // If delivery time is 0, always deliver now
        if (msg->delivery_time.tv_sec == 0) {
            break;
        }
        struct timespec cur_time;
        clock_gettime(CLOCK_MONOTONIC_COARSE, &cur_time);
        double diff = timediff_s(&msg->delivery_time, &cur_time);
        if (diff >= 0) {
            break;
        }
        // Otherwise, move it to the back of the queue
        q->tail->next = msg;
        q->tail = msg;
        q->head = msg->next;
        msg->next = NULL;
        // If it's the only message, return NULL
        if (q->head == msg) {
            if (q->shared) {
                pthread_mutex_unlock(&q->mutex);
            }
            return NULL;
        }
        // Otherwise, move to the next message
        msg = q->head;
        num_checked++;
    } while (num_checked < q->num_msgs);

    // If we've checked all the messages
    if (num_checked == q->num_msgs) {
        if (q->shared) {
            pthread_mutex_unlock(&q->mutex);
        }
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
