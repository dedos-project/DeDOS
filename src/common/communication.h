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
 * Reads a buffer of a given size from a file descriptor
 */
int read_payload(int fd, size_t size, void *buff);

/**
 * Writes a buffer of a given size to a file descriptor
 */
ssize_t send_to_endpoint(int fd, void *data, size_t data_len);

/**
 * Initializes a socket which has been bound to a port
 */
int init_bound_socket(int port);

/**
 * Initializes a socket which has been bound to and is listening on a port
 */
int init_listening_socket(int port);

/**
 * Initializes a socket which is connected to a given address
 */
int init_connected_socket(struct sockaddr_in *addr);

#endif
