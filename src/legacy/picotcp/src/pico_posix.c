/*********************************************************************
   PicoTCP. Copyright (c) 2012-2015 Altran Intelligent Systems. Some rights reserved.
   See LICENSE and COPYING for usage.

   Authors: Andrei Carp, Maarten Vandersteegen
 *********************************************************************/



#include <pthread.h>
#include <semaphore.h>
#include "pico_config.h"
#include "logging.h"

#ifndef PICO_SUPPORT_MUTEX
#define PICO_SUPPORT_MUTEX
#endif 

#ifdef PICO_SUPPORT_MUTEX
/* POSIX mutex implementation */
void *pico_mutex_init(void)
{
    pthread_mutex_t *m;
    m = (pthread_mutex_t *)PICO_ZALLOC(sizeof(pthread_mutex_t));
    pthread_mutex_init(m, NULL);
    log_debug("Initialized a mutex at %p",(void *)m);
    return (pthread_mutex_t *)m;
}

void pico_mutex_destroy(void *mux)
{
    PICO_FREE(mux);
    mux = NULL;
    //log_debug("destroyed a mutex","");
}

void pico_mutex_lock(void *mux)
{
    if (mux == NULL) return;

    pthread_mutex_t *m = (pthread_mutex_t *)mux;
    pthread_mutex_lock(m);
//    printf("thread: %lu locked mutex at %p",(pthread_t)pthread_self(), mux);
}

void pico_mutex_unlock(void *mux)
{
    if (mux == NULL) return;

    pthread_mutex_t *m = (pthread_mutex_t *)mux;
    pthread_mutex_unlock(m);
//    printf("thread: %lu unlocked mutex at %p",(pthread_t)pthread_self(), mux);
}

void pico_mutex_deinit(void *mux)
{
    pico_mutex_destroy(mux);
}

#endif

#ifdef PICO_SUPPORT_THREADING
/* POSIX semaphore implementation */
void *pico_sem_init(void)
{
    sem_t *s;
    s = (sem_t *)PICO_ZALLOC(sizeof(sem_t));
    sem_init(s, 0, 0);
    return s;
}

void pico_sem_destroy(void *sem)
{
    PICO_FREE(sem);
    sem = NULL;
}

void pico_sem_post(void *sem)
{
    if (sem == NULL) return;

    sem_t *s = (sem_t *)sem;
    sem_post(s);
}

int pico_sem_wait(void *sem, int timeout)
{
    struct timespec t;
    if (sem == NULL) return 0;

    sem_t *s = (sem_t *)sem;

    if (timeout < 0) {
        sem_wait(s);
    } else {
        clock_gettime(CLOCK_REALTIME, &t);
        t.tv_sec += timeout / 1000;
        t.tv_nsec += (timeout % 1000) * 1000000;
        if (sem_timedwait(s, &t) == -1)
            return -1;
    }

    return 0;
}

/* POSIX thread implementation */
void *pico_thread_create(void *(*routine)(void *), void *arg)
{
    pthread_t *thread;
    thread = (pthread_t *)PICO_ZALLOC(sizeof(pthread_t));

    if (pthread_create(thread, NULL, routine, arg) == -1)
        return NULL;

    return thread;
}
#endif  /* PICO_SUPPORT_THREADING */
