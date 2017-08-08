/**
 * msu_queue.h
 *
 * Will contain declarations for functions relevant to enqueuing and dequeuing of data to MSUs
 * For the moment, some functionality may be in `generic_msu_queue.h` instead while porting
 * is underway
 */
#ifndef MSU_QUEUE_H_
#define MSU_QUEUE_H_
#include <inttypes.h>
#include <unistd.h>

// Forward declarations for circular dependency
struct generic_msu;
struct generic_msu_queue_item;

// NOTE: Created this file to achieve some sort of privacy on the functions.
// Prior to now there was no .c file for msu queues.
// -IMP

#define MAX_PATH_LEN 8

struct composite_key {
    uint64_t a;
    uint64_t b;
    uint64_t c;
};

struct queue_key {
    struct composite_key key;
    size_t key_len;
    int32_t id;
};

/** For holding the path that a queue item has traversed */
struct msu_path_element {
    int type_id;
    int msu_id;
    uint32_t ip_address;
};

void set_queue_key(void *src, ssize_t src_len, struct queue_key *dst);

/**
 * Adds an item to the path recorded inside an queue_item
 */
void add_to_msu_path(struct generic_msu_queue_item *queue_item, 
                     int type_id, int id, uint32_t ip_address);


struct msu_path_element *get_path_history(struct generic_msu_queue_item *queue_item, 
                                          int reverse_index);


/**
 * Creates a new MSU queue item and enqueues it on the appropriate MSU.
 * This function is meant to be used only for the introduction of a queue item
 * into the stack, and not for subsequently passing queue items between MSUs
 * (which should just use the return of the msu_receive function)
 */
int initial_msu_enqueue(void *buffer, size_t buffer_len, uint32_t id, struct generic_msu *dest);
#endif
