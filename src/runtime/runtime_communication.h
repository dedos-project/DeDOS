/**
 * @file runtime/runtime_communication.h
 *
 * Socket-handling between runtimes
 */
#ifndef RUNTIME_COMMUNICATION_H_
#define RUNTIME_COMMUNICATION_H_
#include "inter_runtime_messages.h"

#include <unistd.h>
#include <stdbool.h>
#include <netinet/ip.h>

/**
 * Sends a message to the peer runtime with the provided id
 * @param runtime_id The ID of the runtime to which the message is to be sent
 * @param hdr The header of the inter-runtime message
 * @param payload The message to be sent
 * @returns 0 on success, -1 on error
 */
int send_to_peer(unsigned int runtime_id, struct inter_runtime_msg_hdr *hdr, void *payload);

/**
 * Innitiates a connection to a runtime peer with the given ID at the given address.
 * Only to be called from the output thread
 * @returns 0 on success, -1 on error
 */
int connect_to_runtime_peer(unsigned int id, struct sockaddr_in *addr);

/**
 * Adds the file descriptor to the list of current runtime peers
 */
int add_runtime_peer(unsigned int runtime_id, int fd);

/**
 * Initializes the socket listening for incoming connections
 */
int init_runtime_socket(int listen_port);

/**
 * Reads a message off of the provided file descriptor as if it is coming from a runtime peer
 */
int handle_runtime_communication(int fd);

#endif
