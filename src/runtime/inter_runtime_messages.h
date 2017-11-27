/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania

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
 * @file inter_runtime_messages.h
 *
 * Definitions of the message types that can be passed between runtimes
 */

#ifndef INTER_RUNTIME_MESSAGES_H_
#define INTER_RUNTIME_MESSAGES_H_
#include "msu_type.h"

#include <stdlib.h>

/**
 * These message types can be passed between runtimes
 */
enum inter_runtime_msg_type {
    /** Set to 0 to catch cases where message type has not been set */
    UNKNOWN_INTER_RT_MSG,
    /** Payload type: inter_runtime_init_msg */
    INTER_RT_INIT,
    /** Paylaod type: output of ::serialize_msu_msg */
    RT_FWD_TO_MSU,
};

/**
 * Header for messages to runtime from another runtime
 */
struct inter_runtime_msg_hdr {
    enum inter_runtime_msg_type type;
    unsigned int target; /**< MSU ID or thread ID depending on message type*/
    size_t payload_size;
};

/**
 * Sent to a newly-connected runtime to establish ID
 */
struct inter_runtime_init_msg {
    unsigned int origin_id; /**< ID of the sending runtime */
};

#endif
