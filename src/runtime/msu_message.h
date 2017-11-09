/**
 * @file msu_message.h
 *
 * Messages passed to MSUs
 */

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
    /** Used to mark a route ID when storing state.
     * (will help to enable state migration within a specific group at a later time) */
    int group_id;
};

/** Item in the chain of history kept track of in each MSU */
struct msu_provinance_item {
    unsigned int type_id;
    unsigned int msu_id;
    unsigned int runtime_id;
};

/** The maximum path length recorded for a messages path through MSUs */
#define MAX_PATH_LEN 8

/** Keeps track of which MSUs have seen a given message header */
struct msg_provinance {
    /** The first MSU to see this message */
    struct msu_provinance_item origin;
    /** The path of MSUs this message has traversed */
    struct msu_provinance_item path[MAX_PATH_LEN];
    /** The current length of msg_provinance::path */
    int path_len;
};

/**
 * Header for messages passed to MSUs.
 * Keeps track of message history, route- and state-keys
 */
struct msu_msg_hdr {
    /** Routing/state key */
    struct msu_msg_key key;
    /** Message history */
    struct msg_provinance provinance;
#ifdef DEDOS_PROFILER
    /** Whether this message is to be profiled */
    bool do_profile;
#endif
};

/**
 * A message that is to be delivered to an instance of an MSU
 */
struct msu_msg {
    struct msu_msg_hdr hdr;
    size_t data_size; /**< Payload size */
    void *data;       /**< Payload */
};

/**
 * @returns the type of the MSU which last sent this message header
 */
unsigned int msu_msg_sender_type(struct msg_provinance *prov);

/**
 * Adds a new item to the path of MSUs taken within the mesasge provinance in the header.
 * @param prov Existing provinance structure
 * @param sender The sending MSU, to be added to the provinance
 * @return 0 on success, -1 on error
 */
int add_provinance(struct msg_provinance *prov,
                   struct local_msu *sender);

/**
 * Initializes and resets a message header, storing a copy of the provided key.
 * Note: key can be a local variable. Its value is copied.
 * @param hdr The header to reset
 * @param key The key to set a copy of in the header
 * @return 0 on success, -1 on error
 */
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

/**
 * Frees both the message and message data
 */
void destroy_msu_msg_and_contents(struct msu_msg *msg);

/**
 * Reads an MSU message of the given size from the provided file descriptor.
 * Utilizes the msu_type::deserialize function of the provided MSU if applicable,
 * otherwise just reads the provided payload.
 * @param msu The msu from which to take the deserialization function
 * @param fd The file descriptor from which to read the message
 * @param size The size of the message available in the file descriptor
 */
struct msu_msg *read_msu_msg(struct local_msu *msu, int fd, size_t size);

/**
 * Schedules an MSU message to be delivered after `interval` time has passed
 * @param q The queue into which the message is to be placed
 * @param data The message to be delivered
 * @param interval The amount of time which must elapse before the message is to be delivered
 * @return 0 on success, -1 on error
 */
int schedule_msu_msg(struct msg_queue *q, struct msu_msg *data, struct timespec *interval);

/**
 * Enqueues a message for immediate delivery.
 * @param q The queue into which the message is to be placed
 * @param data The message to be delivered
 * @return 0 on success, -1 on error
 */
int enqueue_msu_msg(struct msg_queue *q, struct msu_msg *data);

/**
 * Dequeues an MSU message from the provided message queue.
 * @return the dequeued message on success, NULL if the message is not available,
 * or is not an MSU_MSG
 */
struct msu_msg *dequeue_msu_msg(struct msg_queue *q);

/**
 * Converts an MSU message into a serializes stream of bytes.
 * @param msg The message to be serialized
 * @param dst_type The MSU type to which the message is to be delivered.
 *                 Serialization utilizes msu_type::serialize if non-NULL.
 * @param size_out An output argument, set to the size of the newly serialized message
 * @returns A newly-allocated (must be freed externally) serialized copy of `msg`
 */
void *serialize_msu_msg(struct msu_msg *msg,
                        struct msu_type *dst_type, size_t *size_out);

/**
 * Creates an MSU message with the appropriate header, data size, and data.
 * @returns The newly-allocated MSU message
 */
struct msu_msg *create_msu_msg(struct msu_msg_hdr *hdr,
                               size_t data_size,
                               void *data);
#endif
