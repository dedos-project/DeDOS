#ifndef MSU_MESSAGE_H
#define MSU_MESSAGE_H
#include "message_queue.h"

struct msu_msg_key {
    uint32_t key;
};

struct msg_provinance {
};

struct msu_msg {
    struct msu_msg_key key;
    struct msg_provinance provinance;
    size_t data_size;
    void *data;
};

int enqueue_msu_msg(struct msg_queue *q, struct msu_msg *data);
struct msu_msg *dequeue_msu_msg(struct msg_queue *q);

#endif
