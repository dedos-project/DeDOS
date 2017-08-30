#ifndef DEDOS_THREADS_H_
#define DEDOS_THREADS_H_
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>

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
    // ???: uint32_t size;
    // ???: uint32_t max_msgs;
    // ???: uint32_t max_size;
    struct dedos_msg *head;
    struct dedos_msg *tail;
    pthread_mutex_t mutex;
    bool shared;
    // ???: uint16_t overhead;
};

struct dedos_msg *dequeue_msg(struct msg_queue *q);
int enqueue_msg(struct msg_queue *q, struct dedos_msg *msg);
int init_msg_queue(struct msg_queue *q);
#endif //DEDOS_THREADS_H_
