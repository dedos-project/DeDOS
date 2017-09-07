/**
 * @file msu_type.h
 *
 * Defines a type of MSU, callback functions
 */
#ifndef MSU_TYPE_H
#define MSU_TYPE_H
#include "routing.h"
#include "ctrl_runtime_messages.h"

#include <stdint.h>

// Need forward declaration due to circular dependency
struct local_msu;
struct msu_msg;

/**
 * Defines a type of MSU. This information (mostly callbacks)
 * is shared across all MSUs of the same type.
 */
struct msu_type{
    /** Name for the msu type */
    char *name;

    /* removed: int layer? */
    /* removed: unsigned int proto_number*/

    /** Numerical identifier for the MSU. */
    unsigned int id;

    /** Constructor for an MSU_type itself, should initialize whatever context
     * all MSUs of this type might need to rely upon.
     * Can be set to NULL if no additional init must occur.
     * @param type MSU type under construction
     * @return 0 on success, -1 on error
     */
    int (*init_type)(struct msu_type *type);

    /** Type-specific construction function.
     * Generic construction for an MSU is always handled by local_msu.
     * Can be set to NULL if no additional initialization must occur.
     * If this is non-NULL it provides *additional* initialization,
     * such as construction of msu state.
     * @param self MSU to be initialized (structure is already allocated)
     * @param initial_data initial information passed to the runtime by the global controller
     * @return 0 on success, -1 on error
     */
    int (*init)(struct local_msu *self, struct msu_init_data *initial_data);

    /**
     * Type-specific destructor that frees any internal data or state.
     * Can be NULL if no additional freeing must occur
     * @param self MSU to be destroyed
     */
    void (*destroy)(struct local_msu *self);

    /** Handles the receiving of data sent from other MSUs.
     * This field can **not** be set to NULL, and must be defined
     * for each msu_type.
     * @param self MSU receieving data
     * @param input_data data to receive from previous MSU
     * @return MSU type-id of next MSU to receive data, 0 if no rcpt, -1 on err
     */
    int (*receive)(struct local_msu *self, struct msu_msg *data);

    /* Move state within same process and physical machine, like pointers.
     * Runtime should call this.
     * @param data ???
     * @param optional_data ???
     * This never appeared to be used in the current codebase, so I am removing it
     * till we know the exact requirements
     */
     // removed: int (*same_machine_move_state)(void *data, void *optional_data);

    /** Choose which MSU of **this** type the **previous** MSU will route to.
     * Can **not** be NULL.
     * @param type MSU type of endpoint
     * @param sender MSU sending the information
     * @param data data to be sent, in case destination depends on specifics of data
     * @return msu_endpoint destination
     */
    struct msu_endpoint *(*route)(struct msu_type *type, struct local_msu *sender,
                                  struct msu_msg *data);

    /** TODO: Serialization function */
    size_t (*serialize)(struct msu_type *type, struct msu_msg *input,
                        void **output);

    /** Deserializes data received from remote MSU and enqueues the
     * message payload onto the msu queue.
     * TODO: Remove enqueing from function duty.
     * Can **not** be NULL. Set to default_deserialize for default behavior.
     * @param self MSU receiving data
     * @param msg message being received
     * @param buf ???? Unused?
     * @param bufsize Size of the buffer being received
     * @return 0 on success, -1 on error
     */
    int (*deserialize)(struct local_msu *self, void *data, int data_len,
                       struct msu_msg *output);

    /** Generates an ID for the incoming queue item.
     * Only necessary for input MSUs, though technically
     * IDs could be reassigned as the queue item traverses the stack.
     * @param self MSU receiving data
     * @param queue_item item to be passed to subsequent MSUs
     */
    void (*generate_id)(struct local_msu *self,
                        struct msu_msg *queue_item);
};

/** Initializes an MSU type, optionally calling init function */
int init_msu_type_id(unsigned int type_id);
/** Gets the MSU type with the provided ID */
struct msu_type *get_msu_type(int id);

#endif // MSS_TYPE_H
