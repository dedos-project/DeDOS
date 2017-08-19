#ifndef LOCAL_MSU_H_
#define LOCAL_MSU_H_
#include "msu_type.h"

#define MAX_INIT_DATA_LEN 32


struct local_msu {

    /** Pointer to struct containing information shared across
     * all instances of this type of MSU.
     * Includes type-specific MSU functions
     */
    struct msu_type *type;

    /** Routing table pointer, containing all destination MSUs
     * TODO: Update route set to be more intelligent
     */
    struct route_set *routes;

    // ???: struct generic_msu *next (Linked list for MSU pool)
    
    /** Unique ID for a local MSU */
    unsigned int id;

    /** The number of items that should be dequeued on this MSU each tick */
    unsigned int scheduling_weight;

    /** Input queue to MSU */
    struct msg_queue msu_queue;

    /** Queue for control updates (e.g. routing changes?)
     * ???: What other control updates can an MSU receive? Could they be handled generically
     */
    struct msg_queue ctrl_queue;

    /** Keyed state structure, accessible through queue item IDs */
    struct msu_state *item_state;

    /** State related to the entire instance of the MSU, not just individual items
     * RENAMED: internal_state -> msu_state
     */
    void *msu_state;

    // MOVED: routing_state can be moved to msu_state
}

struct local_msu *init_msu(unsigned int id, struct msu_type *type, 
                           struct worker_thread *thread, struct msu_init_data *data);

