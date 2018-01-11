/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
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
