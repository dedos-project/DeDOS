#include <stdbool.h>

#ifndef LOG_DEDOS_THREADS
#define LOG_DEDOS_THREADS
#endif

#define MAX_THREAD_ID 32

static int n_threads = 0;
static struct dedos_thread *threads[MAX_THREAD_ID];

typedef int (*dedos_thread_fn)(struct dedos_thread *, struct main_thread *);

struct thread_msg *dequeue_thread_msg(struct dedos_thread *thread) {
    struct dedos_msg *msg = dequeue_msg(&thread->queue);
    if (msg == NULL) {
        return NULL;
    }
    if (msg->data_size != sizeof(struct thread_msg)) {
        log_error("Attempted to dequeue non-thread msg from thread queue");
        return NULL;
    }

    struct thread_msg *thread_msg = msg->data;
    free(msg);
    return thread_msg;
}

int init_thread_stat_items(int id) {
    int rtn = init_stat_item(CTX_SWITCHES, (unsigned int)id);
    if (rtn < 0) {
        log_error("Error initializing CTX_SWITCHES");
        return -1;
    }
    return 0;
}

/** 
 * Initializes a dedos_thread structure.
 * TO BE CALLED ON FROM RUNNING THREAD
 */
int init_dedos_thread(struct dedos_thread *thread, 
                      enum thread_behavior behavior,
                      int id) {
    if (id > MAX_THREAD_ID) {
        log_error("Cannot start thread with id > %d", MAX_THREAD_ID);
        return -1;
    }
    thread->tid = pthread_self();
    thread->id = id; 
    thread->behavior = behavior;
    
    if (init_thread_stat_items(thread->id) != 0) {
        log_warn("Error initializing thread statistics");
    }
    
    if (init_msg_queue(&thread->queue) != 0) {
        log_error("Error initializing message queue for main thread");
        return -1;
    }
    return 0;
}

struct thread_init {
    dedos_thread_fn thread_fn;
    struct dedos_thread *self;
    struct dedos_thread *main_thread;
    enum thread_behavior behavior;
    int id;
    sem_t start_sem;
}

static void *dedos_thread_starter(void *thread_init_v) {
    struct thread_init *init = thread_init_v;

    // Have to get things out of the thread_init, because
    // it will be destroyed externally once sem_post() is complete
    struct dedos_thread *thread = init->self;
    dedos_thread_fn thread_fn = init->thread_fn;
    struct dedos_thread *main_thread = init->main_thread
    int rtn = init_dedos_thread(thread, init->behavior);
    sem_post(&init->start_sem);
    if (rtn < 0) {
        return (intptr_t) -1;
    }
    return (intptr_t)init->thread_fn(thread, main_thread);
}


static int start_dedos_thread(dedos_thread_fn start_routine,
                              enum thread_behavior behavior, 
                              int id;
                              struct dedos_thread *thread, 
                              void *data) {
    struct thread_init init = {
        .thread_fn = start_routine;
        .self = thread;
        .behavior = behavior;
        .id = id;
        .start_data = data;
    }
    if (sem_init(&init->sem, 0, 0)) {
        log_perror("Error initializing semaphore for dedos thread");
        return -1;
    }
    log_custom(LOG_DEDOS_THREADS, "Waiting on thread %d to start", id);
    int rtn = pthread_create(thread->pthread, NULL,
                             dedos_thread_starter, (void*)&init);
    if (rtn < 0) {
        log_error("pthread_create failed with errno: %d", rtn);
        return -1;
    }
    if (sem_wait(&init->sem) != 0) {
        log_perror("Error waiting on thread start semaphore");
        return -1;
    }
    log_custom(LOG_DEDOS_THREADS, "Thread %d started successfully", id);
    sem_destroy(&init->sem);
    return 0;
}



