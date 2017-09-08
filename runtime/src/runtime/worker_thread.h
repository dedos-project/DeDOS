#ifndef WORKER_THREAD_H_
#define WORKER_THREAD_H_
#include "dedos_threads.h"

// Forward declaration for circular dependency
struct local_msu; 

struct worker_thread {
    struct dedos_thread *thread;
    int n_msus;
    struct local_msu *msus[MAX_MSU_PER_THREAD];
};

struct worker_thread *get_worker_thread(int id);
int create_worker_thread(unsigned int thread_id, enum blocking_mode mode);

#endif
