/**
 * @file worker_thread.h
 *
 * Threads that hold MSUs
 */

#ifndef WORKER_THREAD_H_
#define WORKER_THREAD_H_
#include "dedos_threads.h"

// Forward declaration for circular dependency
struct local_msu;

/** An entry in the linked list of timeouts */
struct timeout_list {
    struct timespec time;
    struct timeout_list *next;
};

/** Representation of a thread that holds MSUs, messages, and waits on a semaphore */
struct worker_thread {
    /** The underlying dedos_thread */
    struct dedos_thread *thread;
    /** The number of msus on the thread */
    int n_msus;
    /** Lock protecting the worker_thread::exit_signal */
    pthread_mutex_t exit_lock;
    int exit_signal;
    /** The MSUs on the thread */
    struct local_msu *msus[MAX_MSU_PER_THREAD];
    /** The times at which the sem_trywait on on the local semaphore should be released*/
    struct timeout_list *timeouts;
};

/**
 * @returns the worker thread with the given ID, or NULL if not found
 */
struct worker_thread *get_worker_thread(int id);

/** Signals the given worker thread that it should stop execution and exit its main loop */
void stop_worker_thread(struct worker_thread *thread);
/** Signals all worker threads to stop */
void stop_all_worker_threads();

/**
 * Starts a new worker thread with the given thread ID and pinned/unpinned status.
 * Blocks until thread has been started
 */
int create_worker_thread(unsigned int thread_id, enum blocking_mode mode);

/**
 * Removes an MSU from the list of MSUs within its thread
 * @returns 0 on success, -1 on error
 */
int unregister_msu_with_thread(struct local_msu *msu);

/**
 * Registers an MSU as one that should be run on its assigned thread
 * @returns 0 on success, -1 on error
 */
int register_msu_with_thread(struct local_msu *msu);

/**
 * Signals that the given thread should break when waiting on its semaphore once
 * `interval` time has passed
 * @returns 0 on success, -1 on error
 */
int enqueue_worker_timeout(struct worker_thread *thread, struct timespec *interval);

#endif
