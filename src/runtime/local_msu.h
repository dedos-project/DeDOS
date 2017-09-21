#ifndef LOCAL_MSU_H_
#define LOCAL_MSU_H_
#include "msu_type.h"
#include "message_queue.h"
#include "worker_thread.h"

#define MAX_INIT_DATA_LEN 32

// Forward declaration due to circular dependency
struct msu_msg_hdr;
struct msu_msg_key;

struct local_msu {

    /** Pointer to struct containing information shared across
     * all instances of this type of MSU.
     * Includes type-specific MSU functions
     */
    struct msu_type *type;

    /** Routing table set, containing all destination MSUs */
    struct route_set routes;

    /** Unique ID for a local MSU */
    unsigned int id;

    /** The number of items that should be dequeued on this MSU each tick */
    unsigned int scheduling_weight;

    /** Input queue to MSU */
    struct msg_queue queue;

    /** Keyed state structure, accessible through queue item IDs */
    struct msu_state *item_state;

    /** State related to the entire instance of the MSU, not just individual items
     * renamed: internal_state -> msu_state
     */
    void *msu_state;

    struct worker_thread *thread;
    // removed: routing_state can be moved to msu_state if it has custom routing
};

/** Allocates and creates a new MSU of the specified type and ID on the given thread */
struct local_msu *init_msu(unsigned int id, struct msu_type *type,
                           struct worker_thread *thread, struct msu_init_data *data);
/** Calls type-specific destroy function and frees associated memory */
void destroy_msu(struct local_msu *msu);

int msu_dequeue(struct local_msu *msu);

struct local_msu *get_local_msu(unsigned int id);

int init_call_local_msu(struct local_msu *sender, struct local_msu *dest,
                        struct msu_msg_key *key, size_t data_size, void *data);

int call_local_msu(struct local_msu *sender, struct local_msu *dest,
                   struct msu_msg_hdr *hdr, size_t data_size, void *data);

int call_msu(struct local_msu *sender, struct msu_type *dst_type,
             struct msu_msg_hdr *hdr, size_t data_size, void *data);

int init_call_msu(struct local_msu *sender, struct msu_type *dst_type,
                  struct msu_msg_key *key, size_t data_size, void *data);

#endif
