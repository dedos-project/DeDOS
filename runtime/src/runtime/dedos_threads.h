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

typedef int (*dedos_thread_fn)(struct dedos_thread *thread);

int start_dedos_thread(dedos_thread_fn start_routine,
                       enum blocking_mode mode,
                       int id,
                       struct dedos_thread *thread);

int thread_wait(struct dedos_thread *thread);

#endif // DEDOS_THREADS_H
