#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include "logging.h"
#include "dedos_threads.h"

/** POSIX mutex implementation */
pthread_mutex_t *mutex_init(){
    pthread_mutex_t *m = calloc(1, sizeof(*m));
    pthread_mutex_init(m, NULL);
    return m;
}

void mutex_destroy(pthread_mutex_t *mutex){
    free(mutex);
}

void mutex_lock(pthread_mutex_t *mutex){
    if (mutex == NULL) return;

    pthread_mutex_lock(mutex);
}

void mutex_unlock(pthread_mutex_t *mutex){
    if (mutex == NULL) return;
    
    pthread_mutex_unlock(mutex);
}

void mutex_deinit(pthread_mutex_t *mutex){
    mutex_destroy(mutex);
}

/* POSIX semaphore implementation */
sem_t *dedos_sem_init()
{
    sem_t *s = calloc(1, sizeof(*s));
    sem_init(s, 0, 0);
    return s;
}

void dedos_sem_destroy(sem_t *sem)
{
    free(sem);
}

void dedos_sem_post(sem_t *sem)
{
    if (sem == NULL) return;

    sem_post(sem);
}

int dedos_sem_wait(sem_t *sem, int timeout)
{
    struct timespec t;
    if (sem == NULL) return 0;

    if (timeout < 0) {
        sem_wait(sem);
    } else {
        clock_gettime(CLOCK_REALTIME, &t);
        t.tv_sec += timeout / 1000;
        t.tv_nsec += (timeout % 1000) * 1000000;
        if (sem_timedwait(sem, &t) == -1)
            return -1;
    }

    return 0;
}

/* POSIX thread implementation */
void *dedos_thread_create(void *(*routine)(void *), void *arg)
{
    pthread_t *thread = calloc(1, sizeof(*thread));

    if (pthread_create(thread, NULL, routine, arg) == -1)
        return NULL;

    return thread;
}
