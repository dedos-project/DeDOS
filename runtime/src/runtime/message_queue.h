#ifndef DEDOS_THREADS_H_
#define DEDOS_THREADS_H_
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

enum dedos_msg_type {
    RUNTIME_MSG,
    THREAD_MSG,
    MSU_MSG
};

struct dedos_msg {
    struct dedos_msg *next;
    enum dedos_msg_type type;
    void *data;
    ssize_t data_size;
};

struct msg_queue {
    uint32_t num_msgs;
    struct dedos_msg *head;
    struct dedos_msg *tail;
    pthread_mutex_t mutex;
    bool shared;
    sem_t *sem;
};

struct dedos_msg *dequeue_msg(struct msg_queue *q);
int enqueue_msg(struct msg_queue *q, struct dedos_msg *msg);
int init_msg_queue(struct msg_queue *q, sem_t *sem);
#endif //DEDOS_THREADS_H_
