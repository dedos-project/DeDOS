/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
/**
 * @file message_queue.h
 *
 * Structures and functions for enqueueing and dequeuing general-purpose
 * messages from a queue
 */
#ifndef DEDOS_THREADS_H_
#define DEDOS_THREADS_H_
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

/** Messages can either be for delivery to runtime, thread, or MSU */
enum dedos_msg_type {
    RUNTIME_MSG,
    THREAD_MSG,
    MSU_MSG
};

/** A linked-list entry containing a message */
struct dedos_msg {
    /** The next message in the list, or NULL if N/A */
    struct dedos_msg *next;
    /** Target of delivery: runtime, thread, or MSU */
    enum dedos_msg_type type;
    /** Payload */
    void *data;
    /** Size of payload */
    ssize_t data_size;
    /** Message is ignored until this time (in CLOCK_MONOTONIC_COARSE) has passed */
    struct timespec delivery_time;
};

/** Container for linked list message queue */
struct msg_queue {
    uint32_t num_msgs;      /**< Number of messages currently in the queue */
    struct dedos_msg *head; /**< First entry in the queue */
    struct dedos_msg *tail; /**< Last entry in the queue */
    pthread_mutex_t mutex;  /**< Mutex if the queue is shared */
    bool shared;  /**< Whether the queue needs to be locked (always true at the moment)*/
    sem_t *sem;   /**< Post to this semaphore on each new enqueue */
};

/**
 * Dequeues the first available message from `q`.
 * If there are no messages, or no messages with a suitable delivery time, returns NULL.
 * @return dedos_msg dequeued, or NULL
 */
struct dedos_msg *dequeue_msg(struct msg_queue *q);

/**
 * Enqueues a message to be delivered as soon as possible
 * @return 0 on success, -1 on error
 */
int enqueue_msg(struct msg_queue *q, struct dedos_msg *msg);

/**
 * Schedules a message to be delivered once `interval` time has passed.
 * @returns 0 on success, -1 on error
 */
int schedule_msg(struct msg_queue *q, struct dedos_msg *msg, struct timespec *interval);

/**
 * Initilializes a mesasge queue to have no messages in it, and sets up the mutex and semaphore
 * @returns 0 on success, -1 on error
 */
int init_msg_queue(struct msg_queue *q, sem_t *sem);
#endif //DEDOS_THREADS_H_
