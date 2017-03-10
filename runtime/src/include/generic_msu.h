/**
 * generic_msu.c
 *
 * Contains function for the creation of MSUs, as well as the
 * registration of new MSU types
 */
#ifndef GENERIC_MSU_H
#define GENERIC_MSU_H

#include <stdint.h>
#include "routing.h"
#include "comm_protocol.h"

/** A typedef to represent a generic msu */
typedef struct generic_msu msu_t;
/** Contains information generic to a type of MSU*/
typedef struct msu_type_t msu_type_t;

enum layer {
    DEDOS_LAYER_DATALINK = 2, /* Ethernet only. */
    DEDOS_LAYER_NETWORK = 3, /* IPv4, IPv6, ARP.
                                Arp is there because it communicates with L2 */
    DEDOS_LAYER_TRANSPORT = 4, /* UDP, TCP, ICMP */
    DEDOS_LAYER_APPLICATION = 5,   /* Socket management */
    DEDOS_LAYER_ALL = 6
};

typedef struct msu_stats_t {
    unsigned int queue_item_processed;
    unsigned int memory_allocated;
    //data queue size can directly be queried from msu->q_in->size
} msu_stats_t;

/** Routing function to deliver traffic to a set of MSUs */
struct msu_endpoint *round_robin(msu_type_t *type, msu_t *sender,
                                 msu_queue_item_t *data);

/**
 * Defines a type of MSU. This information (mostly callbacks)
 * is shared across all MSUs of the same type.
 */
struct msu_type_t{
    /** Name for the msu type */
    char *name;
    /** Layer in which the msu operates */
    enum layer layer;
    /** Numerical identifier for the MSU
     * IMP: was msu_type*/
    unsigned int type_id;
    /** IMP: ??? */
    unsigned int proto_number;

    /** Type-specific construction function.
     * Generic construction for an MSU is always handled by generic_msu.
     * If this is non-NULL it provides *additional* initialization,
     * such as construction of msu state.
     * @param self MSU to be initialized (structure is already allocated)
     * @param initial_state initial information passed to the runtime by the global controller
     * @return 0 on success, -1 on error
     */
    int (*init)(msu_t *self, struct create_msu_thread_msg_data  *initial_state);

    /**
     * Type-specific destructor that frees any internal data or state
     * Can be NULL if no additional freeing must occur/
     * @param self MSU to be destroyed
     */
    void (*destroy)(msu_t *self);

    /** Dequeues data from input queue.
     * NOTE: **never** handled by generic_msu, **must** be set in msu_type
     * @param self MSU receieving data
     * @param input_data data to receive from previous MSU
     * @return MSU type-id of next MSU to receive data, 0 if no rcpt, -1 on err
     */
    int (*receive)(msu_t *self, msu_queue_item_t *input_data);

    /** Recv function for control updates
     * Generic receipt of control messages (adding/removing routes) handled
     * by generic_msu. Can be NULL if no other messages need be handled.
     * @param self MSU receieving data
     * @param ctrl_msg message to receieve from global controller
     * @return 0 on success, -1 on error
     */
    int (*receive_ctrl)(msu_t *self, struct msu_control_update *ctrl_msg);

    /** Move state within same process and physical machine, like pointers.
     * Runtime should call this.
     * @param data ???
     * @param optional_data ???
     * This never appeared to be used in the current codebase, so I am removing it
     * till we know the exact requirements
     */
    //int (*same_machine_move_state)(void *data, void *optional_data);

    /** Choose which MSU of **this** type the **previous** MSU will route to.
     * set to round_robin for default behavior.
     * TODO(iped): Move to routing object
     * @param type MSU type of endpoint
     * @param sender MSU sending the information
     * @param data data to be sent, in case destination depends on specifics of data
     * @return msu_endpoint destination
     */
    struct msu_endpoint *(*route)(struct msu_type_t *type, msu_t *sender,
                                  msu_queue_item_t *data);

    /** Enqueues data to a local MSU.
     * Set to default_send_local for default behavior.
     * @param self MSU sending data
     * @param queue_item data to send
     * @param dst MSU receiving data
     * @return 0 on success, -1 on error
     */
    int (*send_local)(msu_t *self, msu_queue_item_t *queue_item,
                      struct msu_endpoint *dst);

    /** Enqueues data to a remote MSU (handles data serialization)
     * Set to default_send_remote for default behavior.
     * @param self MSU sending data
     * @param queue_item data to send
     * @param dst MSU receiving data
     * @return 0 on success, -1 on error
     */
    int (*send_remote)(msu_t *self, msu_queue_item_t *queue_item,
                       struct msu_endpoint *dst);

    /** Deserializes data received from remote MSU and enqueues the
     * message payload onto the msu queue.
     * Set to default_deserialize for default behavior.
     * @param self MSU receiving data
     * @param msg message being received
     * @param buf ???? Unused?
     * @param bufsize Size of the buffer being received
     * @return 0 on success, -1 on error
     */
    int (*deserialize)(msu_t *self, intermsu_msg_t *msg, void *buf,
                       uint16_t bufsize);
};

/**
 * This data struct is used to protect data internal to an MSU.
 * The contents of the struct are defined in generic_msu.c,
 * and as such can not be modified directly.
 */
typedef struct msu_data_t *msu_data_p;

/**
 * Type that wraps all functionality for each implemented MSU.
 *
 * Information specific to an instance of an MSU goes in the top
 * level of the struct.
 * Information generic to a type of MSU goes in msu->type
 */
struct generic_msu{
    /** Pointer to struct containing information about msu type.
     * Includes type-specific MSU functions
     */
    msu_type_t *type;

    /** Routing table pointer, containing all destination MSUs*/
    struct msu_routing_table *rt_table;

    /** NEW ROUTING TABLE */
    //msu_route_t *routing_table;

    /** TODO: IMP: ???*/
    msu_t *next;

    /** Unique id for an implemented MSU */
    int id;

    /** The number of items that should be dequeued on this MSU for each tick.*/
    unsigned int scheduling_weight;
    /** Pointer to statistics struct */
    msu_stats_t stats;

    /** Input queue to the MSU */
    msu_queue_t q_in;
    /** Queue for control updates. (e.g. routing table updates) */
    msu_queue_t q_control;

    /** Protected data field. TODO: Finish implementing*/
    msu_data_p data_p;

    /** Any internal state struct which MSU might need to maintain */
    void *internal_state;

    /** State related to routing. Will eventually be moved to routing object*/
    void *routing_state;
};

/** Malloc's and creates a new MSU of the specified type and id. */
msu_t *init_msu(unsigned int type_id, int msu_id,
                struct create_msu_thread_msg_data *create_action);

/** Destroys an msu, freeing all relevant data. */
void destroy_msu(msu_t *self);

/** Default function to enqueue data onto a local msu. */
int default_send_local(msu_t *src, msu_queue_item_t *data,
                       struct msu_endpoint *dst);
/** Default function to enqueue data onto a remote msu. */
int default_send_remote(msu_t *src, msu_queue_item_t *data,
                        struct msu_endpoint *dst);

/** Deserializes and enqueues data received from remote MSU. */
int default_deserialize(msu_t *self, intermsu_msg_t *msg,
                        void *buf, uint16_t bufsize);

/** Receives and handles dequeued data from another MSU */
int msu_receive(msu_t *self, msu_queue_item_t *input_data);

/** Recieves cmd from the global controller */
int msu_receive_ctrl(msu_t *self, msu_queue_item_t *ctrl_msg);

/** Move state within same process and physical machine, like pointers ??? */
int msu_move_state(void *data, void *optional_data);


/** Allocates custom data in an MSU and returns the alloc'd data*/
void* msu_alloc_data(msu_t *msu, size_t bytes);
/** Frees the custom data associated with an MSU */
void msu_free_data(msu_t *msu);
/** Gets the custom data associated with an MSU */
void *msu_data(msu_t *msu);

/** An MSU registers itself with this so that instances can be created */
void register_msu_type(msu_type_t *msu_type);

#endif
