#ifndef MSU_MESSAGE_H
#define MSU_MESSAGE_H
#include "message_queue.h"
#include "msu_type.h"

/**
 * The composite key is used to store a key of
 * arbitrary length (up to 192 bytes).
 * This is used for storing/retrieving state.
 */
struct composite_key {
    uint64_t k1;
    uint64_t k2;
    uint64_t k3;
};

/**
 * Used to uniquely identify the source of a message,
 * used in state storage as well as routing
 */
struct msu_msg_key {
    /** The full, arbitrary-length, unique key (used in state) */
    struct composite_key key;
    /** The length of the composite key */
    size_t key_len;
    /** A shorter, often hashed id for the key of fixed length (used in routing) */
    int32_t id;
    /** Used to mark a route ID when storing state, enabling state migration
     * within a specific group */
    int group_id;
};

struct msu_provinance_item {
    unsigned int type_id;
    unsigned int msu_id;
    unsigned int runtime_id;
};

#define MAX_PATH_LEN 8

struct msg_provinance {
    struct msu_provinance_item origin;
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

/**
 * Sets the key's ID and composite-ID to be equal to the provided id.
 */
int set_msg_key(int32_t id, struct msu_msg_key *key);

/**
 * Sets the key's composite-ID to the provided value,
 * and sets the key's ID to a hash of the value.
 */
int seed_msg_key(void *seed, size_t seed_size, struct msu_msg_key *key);

void destroy_msu_msg_and_contents(struct msu_msg *msg);

struct msu_msg *read_msu_msg(struct local_msu *msu, int fd, size_t size);

int schedule_msu_msg(struct msg_queue *q, struct msu_msg *data, struct timespec *interval);
int enqueue_msu_msg(struct msg_queue *q, struct msu_msg *data);
struct msu_msg *dequeue_msu_msg(struct msg_queue *q);

void *serialize_msu_msg(struct msu_msg *msg,
                        struct msu_type *dst_type, size_t *size_out);

struct msu_msg *create_msu_msg(struct msu_msg_hdr *hdr,
                               size_t data_size,
                               void *data);
#endif
