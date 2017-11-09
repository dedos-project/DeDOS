/**
 * @file local_msu.h
 * Declares the structures and functions applicable to MSUs on the local machine
 */

#ifndef LOCAL_MSU_H_
#define LOCAL_MSU_H_
#include "msu_type.h"
#include "message_queue.h"
#include "worker_thread.h"

// Forward declaration due to circular dependency
struct msu_msg_hdr;
struct msu_msg_key;

/**
 * The structure that represents an MSU located on the local machine
 */
struct local_msu {

    /**
     * Pointer to struct containing information shared across
     * all instances of this type of MSU.
     * Includes type-specific MSU functions
     */
    struct msu_type *type;

    /**
     * Routing table set, containing all destination MSUs
     * (see routing.h for details)
     */
    struct route_set routes;

    /** Unique ID for a local MSU */
    unsigned int id;

    /** The number of items that should be dequeued on this MSU each tick */
    unsigned int scheduling_weight;

    /** Input queue to MSU */
    struct msg_queue queue;

    /** Keyed state structure, accessible through queue item IDs */
    struct msu_state *item_state;

    /** State related to the entire instance of the MSU, not just individual items */
    void *msu_state;

    /** The worker thread on which this MSU is placed */
    struct worker_thread *thread;
};

/**
 * Allocates and creates a new MSU of the specified type and ID on the given thread
 * @param id ID of the MSU to be created
 * @param type MSU type of the MSU to be created
 * @param thread The thread on which this MSU is to be created
 * @param data Any initial data that is passed to the MSU's specific init function
 * @return The created local MSU, or NULL on error
 */
struct local_msu *init_msu(unsigned int id, struct msu_type *type,
                           struct worker_thread *thread, struct msu_init_data *data);

/**
 * Destroys the MSU, but only if it has no states currently saved.
 * @return 0 on success, 1 if MSU cannot be destroyed
 */
int try_destroy_msu(struct local_msu *msu);

/** Calls type-specific destroy function and frees associated memory */
void destroy_msu(struct local_msu *msu);

/**
 * Dequeus a message from a local MSU and calls its receive function
 * @param msu MSU to dequeue the message from
 * @return 0 on success, -1 on error, 1 if no message existed to be dequeued
 */
int msu_dequeue(struct local_msu *msu);

/**
 * Gets the local MSU with the given ID, or NULL if N/A
 * @param id ID of the MSU to retrieve
 * @return local msu instance, or NULL on error
 */
struct local_msu *get_local_msu(unsigned int id);

#endif
