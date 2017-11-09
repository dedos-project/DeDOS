/**
 * @file rt_controller_messages.h
 * Definitiions of structures for sending messages **from** runtimes **to** controller.
 */
#ifndef RT_CONTROLLER_MESSAGES_H_
#define RT_CONTROLLER_MESSAGES_H_

/**
 * The types of messages which can be sent from controller to runtimes
 */
enum rt_controller_msg_type {
    RT_CTL_INIT, /**< Payload type: rt_controller_init_msg */
    RT_STATS     /**< Payload: output of ::serialize_stat_samples */
};

/**
 * Header for all messages from controller to runtime.
 * Header will alwaays be followed by a payload of type `payload_size`
 */
struct rt_controller_msg_hdr {
    enum rt_controller_msg_type type; /**< Type of payload attached */
    size_t payload_size; /**< Payload size */
};

/**
 * Initialization message, sent to controller to identify runtime upon first connection
 */
struct rt_controller_init_msg {
    /** Unique identifier for the runtime */
    unsigned int runtime_id;
    /** Local IP address with which the runtime listens for other runtimes*/
    uint32_t ip;
    /** Port on which the runtime listens for other runtimes */
    int port;
};

#endif
