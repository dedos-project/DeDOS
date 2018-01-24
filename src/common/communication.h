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
 * @file communication.h
 *
 * Interface for general-purpose socket communication
 */
#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <unistd.h>
#include <stdbool.h>
#include <netinet/ip.h>

/**
 * Reads a buffer of a given size from a file descriptor.
 * Loops until enough  bytes have been read off of the socket, until the socket is closed,
 * or until it has tried more than #MAX_READ_ATTEMPTS times.
 * @param fd The file descriptor from which to read
 * @param size The number of bytes to read off of the file desriptor
 * @param buff The buffer into which the read the bytes
 * @returns 0 on success, -1 on error
 */
int read_payload(int fd, size_t size, void *buff);

/**
 * Writes a buffer of a given size to a file descriptor.
 * Loops on the write call until all of the bytes have been sent or it encounters an error.
 * @param fd The file descriptor to which the data is to be written
 * @param data The buffer to write to the file descriptor
 * @param data_len The size of the buffer to write
 * @returns Number of bytes writen. 0 if no bytes could be written.
 */
ssize_t send_to_endpoint(int fd, void *data, size_t data_len);

/**
 * Initializes a socket which is bound to a given port (and any local IP address).
 * Sets `REUSEPORT` and `REUSEADDR` on the socket.
 * @param port The port to which to bind
 * @returns the file descriptor on success, -1 on error
 */
int _init_bound_socket(int port);
#ifndef init_bound_socket
#define init_bound_socket _init_bound_socket
#endif

/**
 * Initializes a socket which is bound to and listening on the given port.
 * Sets backlog on socket to be equal to #BACKLOG
 * @param port The port on which to listen
 * @return The file descriptor on success, -1 on error
 */
int _init_listening_socket(int port);
#ifndef init_listening_socket
#define init_listening_socket _init_listening_socket
#endif

/**
 * Initializes a socket that is connected to a given address.
 * Blocks until the connection has been estabslished.
 * Sets port and address to be reusable
 * @param addr The address to connect to
 * @returns file descriptor on sucecss, -1 on error
 */
int _init_connected_socket(struct sockaddr_in *addr);
#ifndef init_connected_socket
#define init_connected_socket _init_connected_socket
#endif

#endif
