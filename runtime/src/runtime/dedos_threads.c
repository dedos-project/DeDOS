#include <stdbool.h>

#define MAX_WORKER_THREADS 16

static struct worker_thread worker_threads[MAX_WORKER_THREADS];
static int n_worker_threads = 0;

/** 
 * Initializes a dedos_thread structure.
 * TO BE CALLED ON INITIALIZED THREAD
 */
int init_dedos_thread(struct dedos_thread *thread, 
                      enum thread_behavior behavior) {
    thread->tid = pthread_self();
    thread->behavior = behavior;
    
    if (init_msg_queue(&thread->queue) != 0) {
        log_error("Error initializing message queue for main thread");
        return -1;
    }
    return 0;
}

    

