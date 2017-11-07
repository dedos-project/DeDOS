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
int init_bound_socket(int port);

/**
 * Initializes a socket which is bound to and listening on the given port.
 * Sets backlog on socket to be equal to #BACKLOG
 * @param port The port on which to listen
 * @return The file descriptor on success, -1 on error
 */
int init_listening_socket(int port);

/**
 * Initializes a socket that is connected to a given address.
 * Blocks until the connection has been estabslished.
 * Sets port and address to be reusable
 * @param addr The address to connect to
 * @returns file descriptor on sucecss, -1 on error
 */
int init_connected_socket(struct sockaddr_in *addr);

#endif
