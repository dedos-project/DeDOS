#ifndef MSU_MESSAGE_H
#define MSU_MESSAGE_H
#include "message_queue.h"
#include "msu_type.h"

struct composite_key {
    uint64_t k1;
    uint64_t k2;
    uint64_t k3;
};

struct msu_msg_key {
    struct composite_key key;
    size_t key_len;
    int32_t id;
};

struct msu_provinance_item {
    unsigned int type_id;
    unsigned int msu_id;
    unsigned int runtime_id;
};

#define MAX_PATH_LEN 16

struct msg_provinance {
    struct msu_provinance_item path[MAX_PATH_LEN];
    int path_len;
};

struct msu_msg_hdr {
    struct msu_msg_key key;
    struct msg_provinance provinance;
#ifdef DEDOS_PROFILER
    bool do_profile;
#endif
};

struct msu_msg {
    struct msu_msg_hdr hdr;
    size_t data_size;
    void *data;
};

unsigned int msu_msg_sender_type(struct msg_provinance *prov);

int add_provinance(struct msg_provinance *prov,
                   struct local_msu *sender);

int init_msu_msg_hdr(struct msu_msg_hdr *hdr, struct msu_msg_key *key);

int set_msg_key(int32_t id, struct msu_msg_key *key);
int seed_msg_key(void *seed, size_t seed_size, struct msu_msg_key *key);

void destroy_msu_msg_and_contents(struct msu_msg *msg);

struct msu_msg *read_msu_msg(struct local_msu *msu, int fd, size_t size);

int enqueue_msu_msg(struct msg_queue *q, struct msu_msg *data);
struct msu_msg *dequeue_msu_msg(struct msg_queue *q);

void *serialize_msu_msg(struct msu_msg *msg,
                        struct msu_type *dst_type, size_t *size_out);

struct msu_msg *create_msu_msg(struct msu_msg_hdr *hdr,
                               size_t data_size,
                               void *data);
#endif
