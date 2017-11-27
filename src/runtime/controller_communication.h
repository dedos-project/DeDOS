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
 * @file controller_communication.h
 * Communication with global controller from runtime
 */
#ifndef CONTROLLER_COMMUNICATION_H_
#define CONTROLLER_COMMUNICATION_H_
#include "netinet/ip.h"
#include "ctrl_runtime_messages.h"
#include "rt_controller_messages.h"
#include <stdbool.h>

/**
 * Sends a message to the global controller
 * @param msg Message to send. Must define payload_len.
 * @return -1 on error, 0 on success
 */
int send_to_controller(struct rt_controller_msg_hdr *msg, void *payload);

/**
 * Initilizes a connection with the global controller located at the provided address
 */
int init_controller_socket(struct sockaddr_in *addr);

/**
 * Reads and processes a controller message off of the provided file descriptor.
 * Controller message should start with ctrl_runtime_msg_hdr.
 * @pararm fd File descriptor off of which to read the message
 */
int handle_controller_communication(int fd);

/**
 * **WILL** Send an acknoweledgement of success for a specific message.
 * Not yet used
 */
int send_ack_message(int ack_id, bool success);

/** Checks if `fd` is file descriptor for controller */
bool is_controller_fd(int fd);

/** Samples the relevant statistics and sends them to the controller */
int send_stats_to_controller();

#endif
