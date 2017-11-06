/**
 * @file ctrl_runtime_messages.h
 * Definitions of structures for sending messages **from** the global controller
 * **to** runtimes.
 */
#ifndef CTRL_RUNTIME_MESSAGES_H_
#define CTRL_RUNTIME_MESSAGES_H_
#include "dfg.h"

#include <unistd.h>
#include <stdbool.h>

/**
 * The various top-level types of messages which can be sent from the controller to runtimes.
 * Payload types the structures indicated in the comments below.
 */
enum ctrl_runtime_msg_type {

    // These first messages are meant for delivery to the main thread
    CTRL_CONNECT_TO_RUNTIME, /**< payload: ctrl_add_runtime_msg */
    CTRL_CREATE_THREAD,      /**< payload: ctrl_create_thread_msg */
    CTRL_DELETE_THREAD,      /**< payload: ctrl_delete_thread_msg */
    CTRL_MODIFY_ROUTE,       /**< payload: ctrl_route_msg */

    // TODO: CTRL_REPORT: a message type for requesting runtime status

    // These messages are meant for delivery to worker threads for MSUs
    CTRL_CREATE_MSU,    /**< payload: ctrl_create_msu_msg */
    CTRL_DELETE_MSU,    /**< payload: ctrl_delete_msu_msg */
    CTRL_MSU_ROUTES     /**< payload: ctrl_msu_route_msg */
};

/**
 * All messages sent from controller to runtime are prefixed with this header.
 * This header is always followed by a payload of size ctrl_runtime_msg_hdr::payload_size
 */
struct ctrl_runtime_msg_hdr {
    enum ctrl_runtime_msg_type type; /**< Identifies the type of payload that follows */
    int id;              /**< Id for confirmation message (not implemented)  */
                         // TODO: implement confirmation based on ID of sent message
    int thread_id;       /**< ID of the Thread to which the message is to be delivered.
                              Should be 0 if delivering to the main thread.*/
    size_t payload_size; /**< Payload will be serialized following this struct */
};


///////////////////////
// Main thread messages
///////////////////////

/**
 * Payload for messages of type ::CTRL_CONNECT_TO_RUNTIME.
 * Initiates a connection from the target runtime to the runtime at the specified ip and port.
 */
struct ctrl_add_runtime_msg {
    unsigned int runtime_id; /**< ID of the runtime to connect to */
    uint32_t ip; /**< ip address of the runtime to connect to. IPV4 */
    int port;    /**< port of the runtime to connect to */
};

/**
 * Payload for messages of type ::CTRL_CREATE_THREAD.
 * Creates a new thread with the given ID and pinned/unpinned mode on the target runtime.
 */
struct ctrl_create_thread_msg {
    int thread_id; /**< The ID to give to the created thread */
    enum blocking_mode mode; /**< The mode of the thread.
                               ::BLOCKING_MSU corresponds to ::PINNED_THREAD for now */
    // TODO: Change blocking_mode to thread_mode
};

/**
 * Payload for messages of type ::CTRL_DELETE_THREAD.
 * Deletes the thread with the given ID, as well as any MSUs on that thread
 */
struct ctrl_delete_thread_msg {
    int thread_id; /**< The ID of the thread to be deleted */
};

/**
 * Sub-types for messages of type ::CTRL_MODIFY_ROUTE, which have payload ctrl_route_msg.
 * Each type corresponds to a different operation that can be used to modify route
 * creation and membership.
 * Not to be used to modify route subscription.
 */
enum ctrl_route_msg_type {
    CREATE_ROUTE, /**< Creates a new route */
    DELETE_ROUTE, /**< Deletes a route */
    ADD_ENDPOINT, /**< Adds an endpoint to a route */
    DEL_ENDPOINT, /**< Deletes an endpoint from a route */
    MOD_ENDPOINT  /**< Modifies the key corresponding to a route endpoint */
};

/**
 * Payload for messages of type ::CTRL_MODIFY_ROUTE.
 * For creating routes or adding endpoints _to_ routes (not for adding routes to MSUs).*/
struct ctrl_route_msg {
    enum ctrl_route_msg_type type; /**< sub-type of message */
    int route_id;   /**< Route to which the message applies */
    int type_id;    /**< MSU Type of the route. Only needed if subtype is ::CREATE_ROUTE */
    int msu_id;     /**< ID of MSU to add/delete/modify.
                         Neccessary for subtypes ::ADD_ENDPOINT, ::DEL_ENDPOINT, ::MOD_ENDPOINT */
    int key;        /*<< Key to associate with a route endpoint.
                         Neccessary for subtypes ::ADD_ENDPOINT, ::MOD_ENDPOINT */
    unsigned int runtime_id; /*<< Id of runtime on which endpoint is located.
                                  Neccessary for subtype ::ADD_ENDPOINT */
};

///////////////////////
// Worker thread messages
///////////////////////

/**
 * Payload for messages of type ::CTRL_CREATE_MSU.
 * Note: Thread on which the MSU is created is specified in the ctrl_runtime_msg_hdr
 */
struct ctrl_create_msu_msg {
    int msu_id;     /**< ID of the MSU to create */
    int type_id;    /**< Type ID of the MSU to create */
    struct msu_init_data init_data; /**< Initial data to pass to the MSU */
    // TODO: Replace init_data with payload of variable size
};

/**
 * Payload for messages of type ::CTRL_DELETE_MSU.
 */
struct ctrl_delete_msu_msg {
    bool force; /**< If `true`, forces the deletion of an MSU even if it has existing states*/
    int msu_id; /**< ID of the MSU to be deleted */
};

/**
 * Sub-types for payloads of type ::CTRL_MSU_ROUTES
 */
enum ctrl_msu_route_type {
    ADD_ROUTE, /**< Adds a route to an MSU */
    DEL_ROUTE  /**< Removes a route from an MSU */
};

/**
 * Payload for messages of type ::CTRL_MSU_ROUTES.
 * For adding/removing routes from MSUs.
 * Note: Message should be sent to the thread on which the MSU is placed.
 */
struct ctrl_msu_route_msg {
    enum ctrl_msu_route_type type; /**< Sub-type of message */
    int route_id; /**< ID of route to be added or removed. */
    int msu_id;   /**< MSU to which the route is to be added or removed */
};

#endif
