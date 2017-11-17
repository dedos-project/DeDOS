/**
 * @file dedos_threads.h
 *
 * Control spawned threads with message queue within DeDOS
 */

#ifndef DEDOS_THREADS_H
#define DEDOS_THREADS_H
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "message_queue.h"
#include "dfg.h"

/** Structure representing any thread within DeDOS */
struct dedos_thread {
    /** The underlying pthread */
    pthread_t pthread;
    /** A unique identifier for the thread */
    int id;
    /** [un]pinned */
    enum thread_mode mode;
    /** Queue for incoming message */
    struct msg_queue queue;
    /** Locks thread until a message is available */
    sem_t sem;
    /** For checking if thread should exit */
    pthread_mutex_t exit_lock;
    /** For checking if thread should exit */
    int exit_signal;
    /** For logging thread metrics */
    struct timespec last_metric;
};

/** Returns the dedos_thread with the given ID */
struct dedos_thread *get_dedos_thread(int id);

/**
 * Typedef for an initialization function for a dedos_thread.
 * @param thread The thread under initilization
 * @return An object which is passed into the thread_fn and destroy_fn
 * */
typedef void* (*dedos_thread_init_fn)(struct dedos_thread *thread);

/**
 * Typedef for the function that should be called on a dedos_thread
 * @param thread The thread being executed
 * @param init_output The output of the dedos_thread_init_fn
 * @return should return 0 on success, -1 on error
 */
typedef int (*dedos_thread_fn)(struct dedos_thread *thread, void *init_output);

/**
 * Typedef for the destructor function for a dedos_thread
 * @param thread The thread bneing destroyed
 * @param init_output The output of the dedos_thread_init_fn
 */
typedef void (*dedos_thread_destroy_fn)(struct dedos_thread *thread, void *init_output);

/** Initilizes and starts execution of a dedos_thread.
 * Note: Blocks until the thread is started!
 * @pararm thread_fn The function to execute once the thread starts.
 * @param init_fn The function to execute once, at the start of the thread
 * @param destroy_fn The function to execute when the thread exits
 * @param mode [un]pinned
 * @param id Unique identifier for the thread
 * @param thread The object into which to store the thread
 */
int start_dedos_thread(dedos_thread_fn thread_fn,
                       dedos_thread_init_fn init_fn,
                       dedos_thread_destroy_fn destroy_fn,
                       enum blocking_mode mode,
                       int id,
                       struct dedos_thread *thread);

/** Returns 1 if the exit signal has been triggered, otherwise 0 */
int dedos_thread_should_exit(struct dedos_thread *thread);
/** Sets the exit signal for a thread, causing the main loop to quit */
void dedos_thread_stop(struct dedos_thread *thread);
/** Joins and destroys the dedos_thread */
void dedos_thread_join(struct dedos_thread *thread);
/**
 * To be called from the thread, causes it to wait until a message has been
 * received or the timeout has been reached
 * @param thread The thread to block from
 * @param abs_timeout Time since epoch at which thread should block until
 */
int thread_wait(struct dedos_thread *thread, struct timespec *abs_timeout);

#endif // DEDOS_THREADS_H
