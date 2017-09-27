#ifndef DEDOS_THREADS_H
#define DEDOS_THREADS_H
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "message_queue.h"
#include "dfg.h"

struct dedos_thread {
    pthread_t pthread;
    int id;
    enum thread_mode mode;
    struct msg_queue queue; // Queue for incoming messages
    sem_t sem;
    // TODO: Exit signal
};

struct dedos_thread *get_dedos_thread(unsigned int id);

typedef void* (*dedos_thread_init_fn)(struct dedos_thread *thread);
typedef int (*dedos_thread_fn)(struct dedos_thread *thread, void *init_output);
typedef void (*dedos_thread_destroy_fn)(struct dedos_thread *thread, void *init_output);

int start_dedos_thread(dedos_thread_fn thread_fn,
                       dedos_thread_init_fn init_fn,
                       dedos_thread_destroy_fn destroy_fn,
                       enum blocking_mode mode,
                       int id,
                       struct dedos_thread *thread);

int thread_wait(struct dedos_thread *thread, struct timespec *abs_timeout);

#endif // DEDOS_THREADS_H
