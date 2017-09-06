#include <stdbool.h>
#include <stdlib.h>
#include "dedos_threads.h"
#include "thread_message.h"
#include "logging.h"
#include "stats.h"

#define MAX_DEDOS_THREAD_ID 16
static struct dedos_thread *dedos_threads[MAX_DEDOS_THREAD_ID];

int enqueue_thread_msg(struct thread_msg *thread_msg, struct msg_queue *queue) {
    struct dedos_msg *msg = malloc(sizeof(*msg));
    if (msg == NULL) {
        log_error("Error allocating dedos_msg for thread_msg");
        return -1;
    }
    msg->data_size = sizeof(*thread_msg);
    msg->data = thread_msg;
    int rtn = enqueue_msg(queue, msg);
    if (rtn < 0) {
        log_error("Error enqueueing message on thread message queue");
        return -1;
    }
    log_custom(LOG_THREAD_MESSAGES, "Enqueued thread message %p on queue %p", 
               thread_msg, queue);
    return 0;
}

struct dedos_thread *get_dedos_thread(unsigned int id) {
    if (id > MAX_DEDOS_THREAD_ID) {
        log_error("Requested thread ID too high. Maximum is %d", MAX_DEDOS_THREAD_ID);
        return NULL;
    }
    if (dedos_threads[id] == NULL) {
        log_error("Dedos thread with id %d is not initialized", id);
        return NULL;
    }
    return dedos_threads[id];
}


int init_thread_stat_items(int id) {
    int rtn = init_stat_item(THREAD_CTX_SWITCHES, (unsigned int)id);
    if (rtn < 0) {
        log_error("Error initializing CTX_SWITCHES");
        return -1;
    }
    return 0;
}

/**
 * Initializes a dedos_thread structure.
 */
static int init_dedos_thread(struct dedos_thread *thread,
                      enum thread_mode mode,
                      int id) {
    thread->id = id;
    thread->mode = mode;

    if (init_thread_stat_items(thread->id) != 0) {
        log_warn("Error initializing thread statistics");
    }

    if (init_msg_queue(&thread->queue, &thread->sem) != 0) {
        log_error("Error initializing message queue for main thread");
        return -1;
    }
    log_custom(LOG_DEDOS_THREADS, "Initialized thread %d (mode: %s, addr: %p)",
               id, mode == PINNED_THREAD ? "pinned" : "unpinned", thread);
    dedos_threads[id] = thread;
    return 0;
}

struct thread_init {
    dedos_thread_fn thread_fn;
    struct dedos_thread *self;
    sem_t sem;
};

static void *dedos_thread_starter(void *thread_init_v) {
    struct thread_init *init = thread_init_v;

    // Have to get things out of the thread_init, because
    // it will be destroyed externally once sem_post() is complete
    struct dedos_thread *thread = init->self;
    dedos_thread_fn thread_fn = init->thread_fn;
    sem_post(&init->sem);
    log_custom(LOG_DEDOS_THREADS, "Started thread %d (mode: %s, addr: %p)",
               thread->id, thread-> mode == PINNED_THREAD ? "pinned" : "unpinned", thread);
    return (void*)(intptr_t)thread_fn(thread);
}

int thread_wait(struct dedos_thread *thread) {
    int rtn = sem_wait(&thread->sem);
    if (rtn < 0) {
        log_perror("Error waiting on thread semaphore");
        return -1;
    }
    return 0;
}

int start_dedos_thread(dedos_thread_fn start_routine,
                              enum blocking_mode mode,
                              int id,
                              struct dedos_thread *thread) {
    int rtn = init_dedos_thread(thread, id, mode);
    struct thread_init init = {
        .thread_fn = start_routine,
        .self = thread,
    };
    if (sem_init(&init.sem, 0, 0)) {
        log_perror("Error initializing semaphore for dedos thread");
        return -1;
    }
    log_custom(LOG_DEDOS_THREADS, "Waiting on thread %d to start", id);
    rtn = pthread_create(&thread->pthread, NULL,
                         dedos_thread_starter, (void*)&init);
    if (rtn < 0) {
        log_error("pthread_create failed with errno: %d", rtn);
        return -1;
    }
    if (sem_wait(&init.sem) != 0) {
        log_perror("Error waiting on thread start semaphore");
        return -1;
    }
    log_custom(LOG_DEDOS_THREADS, "Thread %d started successfully", id);
    sem_destroy(&init.sem);
    return 0;
}



