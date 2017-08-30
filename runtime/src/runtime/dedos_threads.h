#ifndef DEDOS_THREADS_H
#define DEDOS_THREADS_H
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "message_queue.h"

   

enum thread_behavior {
    BLOCKING_THREAD = 1,
    NONBLOCK_THREAD = 2
};

struct create_thread_msg {
    int thread_id;
    enum thread_behavior blocking;
};

/**
 * All messages that can be received by main thread or workers
 */
enum thread_msg_type {
    // For main thread
    // REMOVED: SET_INIT_CONFIG = 11,
    // REMOVED: GET_MSU_LIST    = 12,
    // RENAMED: RUNTIME_ENDPOINT_ADD -> ADD_RUNTIME
    // IMP: CONNECT_TO_RUNTIME (initiate, from controller)
    // IMP: vs ADD_RUNTIME (already connected, from sockets)
    ADD_RUNTIME        = 13,
    CONNECT_TO_RUNTIME = 14,
    //???: SET_DEDOS_RUNTIMES_LIST = 20
    CREATE_THREAD = 21,
    DELETE_THREAD = 22,
    SEND_TO_RUNTIME = 1000,
    // TODO: SEND_TO_CONTROLLER,
    MODIFY_ROUTE = 3000,

    // For worker threads
    CREATE_MSU = 2001,
    DELETE_MSU = 2002,
    ADD_ROUTE  = 3001,
    DEL_ROUTE  = 3002,
};

struct thread_msg {
    enum thread_msg_type type;
    int thread_id;
    void *data;
    ssize_t data_size;
};

struct dedos_thread {
    pthread_t pthread;
    int id;
    enum thread_behavior behavior;
    struct msg_queue queue; // Queue for incoming messages
    sem_t sem;
    // TODO: Exit signal
};


struct thread_msg *dequeue_thread_msg(struct msg_queue *queue);
#define MAX_MSU_PER_THREAD 8

typedef int (*dedos_thread_fn)(struct dedos_thread *thread, 
                               struct dedos_thread *main);

int start_dedos_thread(dedos_thread_fn start_routine,
                       enum thread_behavior behavior,
                       int id,
                       struct dedos_thread *thread,
                       struct dedos_thread *main_thread);

int thread_wait(struct dedos_thread *thread);

#endif // DEDOS_THREADS_H
