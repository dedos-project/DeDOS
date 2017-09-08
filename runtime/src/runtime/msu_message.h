#ifndef MSU_MESSAGE_H
#define MSU_MESSAGE_H
#include "message_queue.h"
#include "msu_type.h"

struct msu_msg_key {
    uint32_t key;
};

struct msg_provinance {
};

struct msu_msg_hdr {
    struct msu_msg_key key;
    struct msg_provinance provinance;
};

struct msu_msg {
    struct msu_msg_hdr *hdr;
    size_t data_size;
    void *data;
};

void destroy_msu_msg_contents(struct msu_msg *msg);

struct msu_msg *read_msu_msg(struct local_msu *msu, int fd, size_t size);

int enqueue_msu_msg(struct msg_queue *q, struct msu_msg *data);
struct msu_msg *dequeue_msu_msg(struct msg_queue *q);

void *serialize_msu_msg(struct msu_msg *msg,
                        struct msu_type *dst_type, size_t *size_out);

struct msu_msg *create_msu_msg(struct msu_msg_hdr *hdr,
                               size_t data_size,
                               void *data);
#endif
