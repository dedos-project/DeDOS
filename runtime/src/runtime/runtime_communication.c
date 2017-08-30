/**
 * @file: runtime_communcication.c
 * All communication to and from other runtimes
 */
#include "runtime_communication.h"
#include "logging.h"
#include "communication.h"

#include <unistd.h>

/**
 * Static (global) variable for the socket listening
 * for other runtimes
 */
static int runtime_sock = -1;

struct runtime_peer {
    int fd;
    //TODO: struct dfg_runtime rt
    //TODO: include IP address???
};

/** Maximum number of other runtimes that can connect to this one*/
#define MAX_RUNTIME_ID 32
/**
 * Other runtime peer sockets
 */
static struct runtime_peer runtime_peers[MAX_RUNTIME_ID];

int send_to_peer(unsigned int runtime_id, size_t data_size, void *data) {
    if (runtime_id > MAX_RUNTIME_ID) {
        log_error("Requested peer %d is greater than max runtime ID %d", 
                  runtime_id, MAX_RUNTIME_ID);
        return -1;
    }
    struct runtime_peer *peer = &runtime_peers[runtime_id];
    if (peer->fd <= 0) {
        log_error("Requested peer %d not instantiated", runtime_id);
        return -1;
    }
    int rtn = send_to_endpoint(peer->fd, data, data_size);
    if (rtn <= 0) {
        log_error("Error sending msg to runtime %d", runtime_id);
        return -1;
    }
    return 0;
}


int add_runtime_peer(unsigned int runtime_id, int fd) {
    if (runtime_peers[runtime_id].fd != 0) {
        log_warn("Warning: replacing runtime peer with id %d", runtime_id);
    }
    // TODO: Initialize runtime_peers all to 0
    runtime_peers[runtime_id].fd = fd;
    return 0;
}


static int init_listen_socket(int port) {
    if (runtime_sock > 0) {
        log_error("Runtime socket already initialized");
        return -1;
    }

    runtime_sock = init_bound_socket(port);
    if (runtime_sock < 0) {
        log_error("Error initializing runtime socket");
        return -1;
    }
    return runtime_sock;
}

int init_runtime_socket(int listen_port) {
    int sock = init_listen_socket(listen_port);
    if (sock < 0) {
        log_error("Error initializing runtime socket");
        return -1;
    }
    
    // TODO: init_epoll()
    /*if (add_to_runtime_epoll(sock) != 0) {
        log_error("Error adding runtime socket to epoll");
        return -1;
    }
    */
    return 0;
}
