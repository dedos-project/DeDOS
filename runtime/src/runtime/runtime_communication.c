/**
 * @file: runtime_communcication.c
 * All communication to and from other runtimes
 */

/**
 * Static (global) variable for the socket listening
 * for other runtimes
 */
static int runtime_sock = -1;

struct send_to_peer_msg {
    unsigned int runtime_id;
    size_t data_size;
    void *serialized_data;
}  

struct add_runtime_msg {
    unsigned int runtime_id;
    int fd;
};

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
static struct runtime_peers[MAX_RUNTIME_ID];

static int epoll_fd = -1;

int send_to_peer(struct send_to_peer_msg *msg) {
    if (msg->runtime_id > MAX_RUNTIME_ID) {
        log_error("Requested peer %d is greater than max runtime ID %d", 
                  msg->requested_id, MAX_RUNTIME_ID);
        return -1;
    }
    struct runtime_peer *peer = &runtime_peers[msg->runtime_id];
    if (peer->fd <= 0) {
        log_error("Requested peer %d not instantiated", msg->runtime_id);
        return -1;
    }
    int rtn = send_to_endpoint(peer->fd, msg->data, msg->data_size);
    if (rtn <= 0) {
        log_error("Error sending msg to runtime %d", msg->runtime_id);
        return -1;
    }
    return 0;
}


int add_runtime_peer(struct add_runtime_msg *msg) {
    int id = msg->runtime_id;
    int fd = msg->fd;
    if (runtime_peers[id].fd != 0) {
        log_warn("Warning: replacing runtime peer with id %d", id);
    }
    // TODO: Initialize runtime_peers all to 0
    runtime_peers[id].fd = fd;
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
    int sock = init_listen_socket();
    if (sock < 0) {
        log_error("Error initializing runtime socket");
        return -1;
    }
    if (add_to_runtime_epoll(sock) != 0) {
        log_error("Error adding runtime socket to epoll");
        return -1;
    }
    return 0;
}
