/**
 * @file msu_type.h
 *
 * Defines a type of MSU, including callback and accessor functions
 */
#ifndef MSU_TYPE_H
#define MSU_TYPE_H
#include "routing.h"
#include "ctrl_runtime_messages.h"

#include <stdint.h>

// Need forward declarations due to circular dependency
struct local_msu;
struct msu_msg;

/**
 * Defines a type of MSU. This information (mostly callbacks)
 * is shared across all MSUs of the same type.
 */
struct msu_type{
    /** Name for the msu type */
    char *name;

    /** Numerical identifier for the MSU. */
    unsigned int id;

    /** Constructor for an MSU_type itself, should initialize whatever context
     * all MSUs of this type might need to rely upon.
     * Can be set to NULL if no additional init must occur.
     * @param type MSU type under construction
     * @return 0 on success, -1 on error
     */
    int (*init_type)(struct msu_type *type);

    /** Destructor for an MSU type, should destroy context initialized in `init_type`.
     * @param type The type to be destroyed
     */
    void (*destroy_type)(struct msu_type *type);

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

    /** Type-specific destructor that frees any internal data or state.
     * Can be NULL if no additional freeing must occur
     * @param self MSU to be destroyed
     */
    void (*destroy)(struct local_msu *self);

    /** Handles the receiving of data sent from other MSUs.
     * This field can **not** be set to NULL, and must be defined for each msu_type.
     * @param self MSU receieving data
     * @param msg data to receive from previous MSU
     * @return 0 on success, -1 on error
     */
    int (*receive)(struct local_msu *self, struct msu_msg *msg);

    /** Choose which MSU of **this** type the **previous** MSU will route to.
     * If NULL, defaults to `default_routing()` in runtime/routing_strategies.c
     * @param type MSU type of endpoint
     * @param sender MSU sending the message
     * @param msg data to be sent, in case destination depends on specifics of data
     * @param output Output argument to be filled with the appropriate destination
     * @return 0 on succes, -1 on error
     */
    int (*route)(struct msu_type *type, struct local_msu *sender,
                 struct msu_msg *msg, struct msu_endpoint *output);

    /** Defines serialization protocol for data sent to this MSU type
     * If NULL, assumes no pointers in the msu message, and simply serializes the input for
     * the length defined in the msu message header.
     * @param type The type of MSU to which the serialized message is to be delivered
     * @param input The MSU message to be serialized
     * @param output To be allocated and filled with the serialized MSU message
     * @return Size of the serialized message
     */
    ssize_t (*serialize)(struct msu_type *type, struct msu_msg *input, void **output);

    /** Defines deserialization protocl for data received by this MSU type
     * If NULL, assumes no pointers in the msu message, and simply assigns `input` as the message
     * @param self The MSU to receive the deserialized buffer
     * @param input_size Size of the received buffer
     * @param input Message to be deserialized
     * @param out_size To be filled with the size of the struct deserialized off of the buffer
     * @return The deserialized buffer
     */
    void *(*deserialize)(struct local_msu *self, size_t input_size, void *input, size_t *out_size);
};

/**
 * Calls the type-sepecific constructor for each instantiated MSU type
 */
void destroy_msu_types();

/**
 * Initializes the MSU type with the given ID,
 * calling the custom constructor if appropriate.
 * This function is meant to be called on each MSU type that
 * is defined in the DFG used to initialize the runtime.
 * @return 0 on success, -1 on error
 */
int init_msu_type_id(unsigned int type_id);

/**
 * Gets the MSU type with the provided ID
 * @param id The type ID of the MSU type to retrieve
 * @return The MSU Type with the given ID, NULL in N/A
 */
struct msu_type *get_msu_type(int id);

#endif // MSS_TYPE_H
