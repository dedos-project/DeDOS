/**
 * @file: runtime_communcication.c
 * All communication to and from other runtimes
 */
#include "runtime_communication.h"
#include "logging.h"
#include "communication.h"
#include "msu_message.h"
#include "inter_runtime_messages.h"
#include "local_msu.h"
#include "runtime_dfg.h"
#include "thread_message.h"

#include <stdlib.h>
#include <unistd.h>

/**
 * Static (global) variable for the socket listening
 * for other runtimes
 */
static int runtime_sock = -1;

struct runtime_peer {
    int fd;
    //shouldI: struct dfg_runtime rt
    //shouldI: uint32_t ip_address
};

/** Maximum number of other runtimes that can connect to this one*/
#define MAX_RUNTIME_ID 32
/**
 * Other runtime peer sockets.
 * Structs will be initialized to 0 due to static initialization.
 */
static struct runtime_peer runtime_peers[MAX_RUNTIME_ID];

int send_to_peer(unsigned int runtime_id, struct inter_runtime_msg_hdr *hdr, void *payload) {
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

    int rtn = send_to_endpoint(peer->fd, hdr, sizeof(*hdr));
    if (rtn <= 0) {
        log_error("Error sending header to runtime %d", runtime_id);
        return -1;
    }

    if (hdr->payload_size <= 0) {
        return 0;
    }

    rtn = send_to_endpoint(peer->fd, payload, hdr->payload_size);
    if (rtn <= 0) {
        log_error("Error sending payload to runtime %d", runtime_id);
        return -1;
    }
    return 0;
}


int add_runtime_peer(unsigned int runtime_id, int fd) {
    if (runtime_peers[runtime_id].fd != 0) {
        log_warn("Replacing runtime peer with id %d", runtime_id);
    }
    runtime_peers[runtime_id].fd = fd;
    return 0;
}

int connect_to_runtime_peer(unsigned int id, struct sockaddr_in *addr){
    if (runtime_peers[id].fd != 0) {
        log_warn("Attempting to replace runtime peer with id %d", id);
    }
    int fd = init_connected_socket(addr);
    if (fd < 0) {
        log_error("Could not connect to runtime %u", id);
        return -1;
    }
    runtime_peers[id].fd = fd;

    int local_id = local_runtime_id();
    if (local_id < 0) {
        log_error("Could not send local runtime ID to remote runtime %d", id);
        return -1;
    }

    struct inter_runtime_init_msg msg = {
        .origin_id = local_id
    };

    struct inter_runtime_msg_hdr hdr = {
        .type = INTER_RT_INIT,
        .target = 0,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_peer(id, &hdr, &msg);
    if (rtn < 0) {
        log_error("Could not send initial connection message to peer runtime %d", id);
        return -1;
    }

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
    return sock;
}

static int read_runtime_message_hdr(int fd, struct inter_runtime_msg_hdr *msg) {
    if (read_payload(fd, sizeof(*msg), msg) != 0) {
        log_error("Could not read runtime message header from socket %d", fd);
        return -1;
    }
    return 0;
}

static int process_fwd_to_msu_message(size_t payload_size, int msu_id, int fd) {

    struct msu_msg *msu_msg = read_msu_msg(fd, payload_size);
    if (msu_msg == NULL) {
        log_error("Error reading MSU msg off of fd %d", fd);
        return -1;
    }

    struct local_msu *msu = get_local_msu(msu_id);
    if (msu == NULL) {
        log_error("Error getting MSU with ID %d, requested from runtime fd %d",
                  msu_id, fd);
        destroy_msu_msg(msu_msg);
        return -1;
    }

    int rtn = enqueue_msu_msg(&msu->queue, msu_msg);
    if (rtn < 0) {
        log_error("Error enqueuing inter-msu message to MSU %d from runtime fd %d",
                  msu_id, fd);
        destroy_msu_msg(msu_msg);
        return -1;
    }
    return 0;
}

static int process_init_rt_message(size_t payload_size, int fd) {

    if (payload_size != sizeof(struct inter_runtime_init_msg)) {
        log_warn("Payload size of runtime initialization message does not match init_msg");
    }
    struct inter_runtime_init_msg msg;
    if (read_payload(fd, sizeof(msg), &msg) != 0) {
        log_error("Error reading inter_runtime_init_message from fd %d", fd);
        return -1;
    }

    struct thread_msg *thread_msg = init_runtime_connected_thread_msg(msg.origin_id, fd);
    if (thread_msg == NULL) {
        log_error("Error creating thread message for connection to runtime %d (fd: %d)",
                  msg.origin_id, fd);
        return -1;
    }
    return 0;
}


static int process_runtime_message_hdr(struct inter_runtime_msg_hdr *hdr, int fd) {
    int rtn;
    switch (hdr->type) {
        case RT_FWD_TO_MSU:
            rtn = process_fwd_to_msu_message(hdr->payload_size, hdr->target, fd);
            if (rtn < 0) {
                log_error("Error processing forward message from fd %d", fd);
                return -1;
            }
            return 0;
        case INTER_RT_INIT:
            rtn = process_init_rt_message(hdr->payload_size, fd);
            if (rtn < 0) {
                log_error("Error processing init runtime message from fd %d", fd);
                return -1;
            }
            return 0;
        default:
            log_error("Received unknown message type from fd %d: %d", fd, hdr->type);
            return -1;
    }
}

int handle_runtime_communication(int fd) {
    struct inter_runtime_msg_hdr hdr;
    int rtn = read_runtime_message_hdr(fd, &hdr);

    if (rtn < 0) {
        log_error("Error reading runtime message");
        return -1;
    } else {
        log_custom(LOG_INTER_RUNTIME_COMMUNICATION,
                   "Read message from runtime with fd %d" fd);
    }

    rtn = process_runtime_message_hdr(&hdr, fd);
    if (rtn < 0) {
        log_error("Error processing inter-runtime message from fd %d", fd);
        return -1;
    }
    return 0;
}
