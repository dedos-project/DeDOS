/**
 * @file generic_msu.c
 *
 * Contains function for the creation of MSUs, as well as the
 * registration of new MSU types
 */
#ifndef GENERIC_MSU_H
#define GENERIC_MSU_H

#include <stdint.h>
#include "routing.h"
#include "comm_protocol.h"
#include "generic_msu_queue_item.h"

enum layer {
    DEDOS_LAYER_DATALINK = 2, /* Ethernet only. */
    DEDOS_LAYER_NETWORK = 3, /* IPv4, IPv6, ARP.
                                Arp is there because it communicates with L2 */
    DEDOS_LAYER_TRANSPORT = 4, /* UDP, TCP, ICMP */
    DEDOS_LAYER_APPLICATION = 5,   /* Socket management */
    DEDOS_LAYER_ALL = 6
};

typedef struct msu_stats {
    int queue_item_processed[2];
    int memory_allocated[2];
    //data queue size can directly be queried from msu->q_in->size
} msu_stats;

// Forward declaration so we can use it in msu_type without compiler complaining
struct generic_msu;

/**
 * Defines a type of MSU. This information (mostly callbacks)
 * is shared across all MSUs of the same type.
 */
struct msu_type{
    /** Name for the msu type */
    char *name;
    /** Layer in which the msu operates */
    enum layer layer;
    /** Numerical identifier for the MSU. */
    unsigned int type_id;
    /** IMP: ??? */
    unsigned int proto_number;

    /** Type-specific construction function.
     * Generic construction for an MSU is always handled by generic_msu.
     * Can be set to NULL if no additional initialization must occur.
     * If this is non-NULL it provides *additional* initialization,
     * such as construction of msu state.
     * @param self MSU to be initialized (structure is already allocated)
     * @param initial_state initial information passed to the runtime by the global controller
     * @return 0 on success, -1 on error
     */
    int (*init)(struct generic_msu *self, struct create_msu_thread_msg_data  *initial_state);

    /**
     * Type-specific destructor that frees any internal data or state.
     * Can be NULL if no additional freeing must occur
     * @param self MSU to be destroyed
     */
    void (*destroy)(struct generic_msu *self);

    /** Handles the receiving of data sent from other MSUs.
     * This field can **not** be set to NULL, and must be defined
     * for each msu_type.
     * @param self MSU receieving data
     * @param input_data data to receive from previous MSU
     * @return MSU type-id of next MSU to receive data, 0 if no rcpt, -1 on err
     */
    int (*receive)(struct generic_msu *self, msu_queue_item *input_data);

    /** Handles receiving of data sent specifically from the Global Controller.
     * Generic receipt of control messages (adding/removing routes) 
     * is handled by the generic_msu. 
     * Can be NULL if no other messages need be handled.
     * @param self MSU receieving data
     * @param ctrl_msg message to receieve from global controller
     * @return 0 on success, -1 on error
     */
    int (*receive_ctrl)(struct generic_msu *self, struct msu_control_update *ctrl_msg);

    /* Move state within same process and physical machine, like pointers.
     * Runtime should call this.
     * @param data ???
     * @param optional_data ???
     * This never appeared to be used in the current codebase, so I am removing it
     * till we know the exact requirements
     */
    //int (*same_machine_move_state)(void *data, void *optional_data);

    /** Choose which MSU of **this** type the **previous** MSU will route to.
     * Can **not** be NULL. Set to round_robin for default behavior.
     * TODO(iped): Move to routing object
     * @param type MSU type of endpoint
     * @param sender MSU sending the information
     * @param data data to be sent, in case destination depends on specifics of data
     * @return msu_endpoint destination
     */
    struct msu_endpoint *(*route)(struct msu_type *type, struct generic_msu *sender,
                                  msu_queue_item *data);

    /** Enqueues data to a local MSU.
     * Can **not** be NULL. Set to default_send_local for default behavior.
     * @param self MSU sending data
     * @param queue_item data to send
     * @param dst MSU receiving data
     * @return 0 on success, -1 on error
     */
    int (*send_local)(struct generic_msu *self, msu_queue_item *queue_item,
                      struct msu_endpoint *dst);

    /** Enqueues data to a remote MSU (handles data serialization).
     * Can **not** be NULL. Set to default_send_remote for default behavior.
     * @param self MSU sending data
     * @param queue_item data to send
     * @param dst MSU receiving data
     * @return 0 on success, -1 on error
     */
    int (*send_remote)(struct generic_msu *self, msu_queue_item *queue_item,
                       struct msu_endpoint *dst);

    /** Deserializes data received from remote MSU and enqueues the
     * message payload onto the msu queue.
     * Can **not** be NULL. Set to default_deserialize for default behavior.
     * @param self MSU receiving data
     * @param msg message being received
     * @param buf ???? Unused?
     * @param bufsize Size of the buffer being received
     * @return 0 on success, -1 on error
     */
    int (*deserialize)(struct generic_msu *self, intermsu_msg *msg,
                       void *buf, uint16_t bufsize);
};

/**
 * This data struct is used to protect data internal to an MSU.
 * The contents of the struct are defined in generic_msu.c,
 * and as such can not be modified directly.
 */
typedef struct msu_data *msu_data_p;

/**
 * Type that wraps all functionality for each implemented MSU.
 *
 * Information specific to an instance of an MSU goes in the top
 * level of the struct.
 * Information generic to a type of MSU goes in msu->type
 */
struct generic_msu{
    /** Pointer to struct containing information shared across
     * all instances of this type of MSU.
     * Includes type-specific MSU functions
     */
    struct msu_type *type;

    /** Routing table pointer, containing all destination MSUs*/
    struct msu_routing_table *rt_table;

    /* NEW ROUTING TABLE */
    //msu_route *routing_table;

    /** ??? */
    struct generic_msu *next;

    /** Unique id for an implemented MSU */
    int id;

    /** The number of items that should be dequeued on this MSU for each tick.*/
    unsigned int scheduling_weight;
    /** Pointer to statistics struct */
    msu_stats stats;

    /** Input queue to the MSU */
    msu_queue q_in;
    /** Queue for control updates. (e.g. routing table updates) */
    msu_queue q_control;

    /** Protected data field. TODO: Finish implementing*/
    msu_data_p data_p;

    /** Any internal state struct which MSU might need to maintain.
     * TODO: Normalize across MSUs, creating key-based state structure.
     * Tavish: Not necessarily can be any internal data structure which
     * doesn't have to be key based. */
    void *internal_state;

    /** State related to routing. Will eventually be moved to routing object*/
    void *routing_state;
};

/** Routing function to deliver traffic to a set of MSUs */
struct msu_endpoint *round_robin(struct msu_type *type, struct generic_msu *sender,
                                 msu_queue_item *data);

/** Malloc's and creates a new MSU of the specified type and id. */
struct generic_msu *init_msu(unsigned int type_id, int msu_id,
                    struct create_msu_thread_msg_data *create_action);

/** Destroys an msu, freeing all relevant data. */
void destroy_msu(struct generic_msu *self);

/** Default function to enqueue data onto a local msu. */
int default_send_local(struct generic_msu *src, msu_queue_item *data,
                       struct msu_endpoint *dst);
/** Default function to enqueue data onto a remote msu. */
int default_send_remote(struct generic_msu *src, msu_queue_item *data,
                        struct msu_endpoint *dst);

/** Deserializes and enqueues data received from remote MSU. */
int default_deserialize(struct generic_msu *self, intermsu_msg *msg,
                        void *buf, uint16_t bufsize);

/** Dequeues and processes data received from another MSU. */
int msu_receive(struct generic_msu *self, msu_queue_item *input_data);

/** Dequeues and processes data received from global controller. */
int msu_receive_ctrl(struct generic_msu *self, msu_queue_item *ctrl_msg);

/** Move state within same process and physical machine, like pointers???.*/
int msu_move_state(void *data, void *optional_data);


/** Allocates state data in an MSU and returns the alloc'd data. */
void* msu_alloc_data(struct generic_msu *msu, size_t bytes);
/** Frees the state data associated with an MSU. */
void msu_free_data(struct generic_msu *msu);
/** Gets the state data associated with an MSU. */
void *msu_data(struct generic_msu *msu);

/** An MSU registers itself with this so that instances can be created
 * using the type->id */
void register_msu_type(struct msu_type *type);
/** Get pointer to msu type struct based on type_id */
struct msu_type *msu_type_by_id(unsigned int type_id);
#endif
