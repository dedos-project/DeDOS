#ifndef WORKER_THREAD_H_
#define WORKER_THREAD_H_
#include "dedos_threads.h"

// Forward declaration for circular dependency
struct local_msu;

struct timeout_list {
    struct timespec time;
    struct timeout_list *next;
};

struct worker_thread {
    struct dedos_thread *thread;
    int n_msus;
    pthread_mutex_t exit_lock;
    int exit_signal;
    struct local_msu *msus[MAX_MSU_PER_THREAD];
    struct timeout_list *timeouts;
};

struct worker_thread *get_worker_thread(int id);
void stop_worker_thread(struct worker_thread *thread);
void stop_all_worker_threads();
int create_worker_thread(unsigned int thread_id, enum blocking_mode mode);
int unregister_msu_with_thread(struct local_msu *msu);
int register_msu_with_thread(struct local_msu *msu);
int enqueue_worker_timeout(struct worker_thread *thread, struct timespec *interval);

#endif
