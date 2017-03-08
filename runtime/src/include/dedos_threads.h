#pragma once

#include <semaphore.h>
#include <pthread.h>


/** POSIX mutex implementation */
pthread_mutex_t *mutex_init();

void mutex_destroy(pthread_mutex_t *mutex);

void mutex_lock(pthread_mutex_t *mutex);

void mutex_unlock(pthread_mutex_t *mutex);

void mutex_deinit(pthread_mutex_t *mutex);

/* POSIX semaphore implementation */
sem_t *dedos_sem_init();

void dedos_sem_destroy(sem_t *sem);

void dedos_sem_post(sem_t *sem);

int dedos_sem_wait(sem_t *sem, int timeout);

/* POSIX thread implementation */
void *dedos_thread_create(void *(*routine)(void *), void *arg);
