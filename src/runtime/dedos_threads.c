#define _GNU_SOURCE  // Needed for CPU_SET etc.

#include "dedos_threads.h"
#include "thread_message.h"
#include "logging.h"
#include "rt_stats.h"

#include <stdbool.h>
#include <stdlib.h>
#include <sched.h>

#define MAX_DEDOS_THREAD_ID 16
#define MAX_CORES 16

static struct dedos_thread *dedos_threads[MAX_DEDOS_THREAD_ID];
static int pinned_cores[MAX_CORES];

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
    sem_init(&thread->sem, 0, 0);

    if (init_thread_stat_items(thread->id) != 0) {
        log_warn("Error initializing thread statistics");
    }


    if (init_msg_queue(&thread->queue, &thread->sem) != 0) {
        log_error("Error initializing message queue for main thread");
        return -1;
    }
    log(LOG_DEDOS_THREADS, "Initialized thread %d (mode: %s, addr: %p)",
               id, mode == PINNED_THREAD ? "pinned" : "unpinned", thread);
    dedos_threads[id] = thread;
    return 0;
}

struct thread_init {
    dedos_thread_fn thread_fn;
    dedos_thread_init_fn init_fn;
    dedos_thread_destroy_fn destroy_fn;
    struct dedos_thread *self;
    sem_t sem;
};

static int pin_thread(pthread_t ptid) {
    int cpu_id;
    int num_cpu = sysconf(_SC_NPROCESSORS_ONLN);
    for (cpu_id = 0; cpu_id < num_cpu && pinned_cores[cpu_id] == 1; cpu_id++);
    if (cpu_id == num_cpu) {
        log_warn("No cores available to pin thread");
        return -1;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    int s = pthread_setaffinity_np(ptid, sizeof(cpuset), &cpuset);
    if (s != 0) {
        log_warn("pthread_setaffinity_np returned error %d", s);
        return -1;
    }
    pinned_cores[cpu_id] = 1;
    log(LOG_DEDOS_THREADS, "Successfully pinned pthread %d", (int)ptid);
    return 0;
}


static void *dedos_thread_starter(void *thread_init_v) {
    struct thread_init *init = thread_init_v;

    // Have to get things out of the thread_init, because
    // it will be destroyed externally once sem_post() is complete
    struct dedos_thread *thread = init->self;
    dedos_thread_fn thread_fn = init->thread_fn;
    dedos_thread_destroy_fn destroy_fn = init->destroy_fn;

    void *init_rtn = NULL;
    if (init->init_fn) {
        init_rtn = init->init_fn(thread);
    }

    if (thread->mode == PINNED_THREAD) {
        pin_thread(thread->pthread);
        log_warn("Could not pin thread %d", thread->id);
    }

    sem_post(&init->sem);
    log(LOG_DEDOS_THREADS, "Started thread %d (mode: %s, addr: %p)",
               thread->id, thread-> mode == PINNED_THREAD ? "pinned" : "unpinned", thread);

    int rtn = thread_fn(thread, init_rtn);
    log(LOG_DEDOS_THREADS, "Thread %d ended.", thread->id);

    if (destroy_fn) {
        destroy_fn(thread, init_rtn);
    }

    return (void*)(intptr_t)rtn;
}

int thread_wait(struct dedos_thread *thread, struct timespec *abs_timeout) {
    int rtn;
    if (abs_timeout == NULL) {
        rtn = sem_wait(&thread->sem);
    } else {
        rtn = sem_timedwait(&thread->sem, abs_timeout);
    }
    if (rtn < 0 && errno != ETIMEDOUT) {
        log_perror("Error waiting on thread semaphore");
        exit(-1);
        return -1;
    }
    return 0;
}

int start_dedos_thread(dedos_thread_fn thread_fn,
                       dedos_thread_init_fn init_fn,
                       dedos_thread_destroy_fn destroy_fn,
                       enum blocking_mode mode,
                       int id,
                       struct dedos_thread *thread) {
    int rtn = init_dedos_thread(thread, mode, id);
    struct thread_init init = {
        .thread_fn = thread_fn,
        .init_fn = init_fn,
        .destroy_fn = destroy_fn,
        .self = thread
    };
    if (sem_init(&init.sem, 0, 0)) {
        log_perror("Error initializing semaphore for dedos thread");
        return -1;
    }
    log(LOG_DEDOS_THREADS, "Waiting on thread %d to start", id);
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
    log(LOG_DEDOS_THREADS, "Thread %d started successfully", id);
    sem_destroy(&init.sem);
    return 0;
}



