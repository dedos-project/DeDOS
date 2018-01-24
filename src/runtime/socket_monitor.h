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
 * @file socket_monitor.h
 *
 * Monitors an incoming port for messages from runtime or controller
 */

#ifndef SOCKET_MONITOR_H_
#define SOCKET_MONITOR_H_

/**
 * Adds the global controller to be monitored by the socket monitor
 * @param fd the file decriptor of the global controller
 * @return 0 on success, -1 on error
 */
int monitor_controller_socket(int fd);

/**
 * Adds a runtime to be monitored by the socket monitor
 * @param fd the file descriptor of the new runtime
 * @return 0 on success, -1 on error
 */
int monitor_runtime_socket(int fd);

/** Connects to the global controller at the provided address */
int initialize_controller_communication(int local_port, struct sockaddr_in *ctrl_addr);

/**
 * Starts (blocking) the socket monitor, listening on the port provided to connect_to_controller
 */
int run_socket_monitor();

#endif
