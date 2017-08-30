/**
 * @file: controller_communications.c
 * All communiction with the global controller
 */
#include "controller_communication.h"
#include "logging.h"
#include "socket_monitor.h"
#include "dedos_threads.h"
#include "thread_message.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

/**
 * Static (global) variable to hold the socket
 * connecting to the global controller
 */
static int controller_sock = -1;

/**
 * Sends a message to the global controller
 * @param msg Message to send. Must define payload_len.
 * @return -1 on error, 0 on success
 */
int send_to_controller(struct rt_controller_msg *msg, void *data) {
    // TODO: Defaine dedos_control_msg
    size_t buf_len = sizeof(*msg) + msg->payload_size;
    char buf[buf_len];
    memcpy(buf, msg, sizeof(*msg));
    memcpy(buf + sizeof(*msg), data, msg->payload_size);
    if (send(controller_sock, buf, buf_len, 0) == -1) {
        log_error("Failed to send message to global controller");
        return -1;
    }
    return 0;
}


/**
 * Initializes a connection to the global controller
 * @returns 0 on success, -1 on error
 */
static int connect_to_controller(struct sockaddr_in *addr) {

    if (controller_sock != -1) {
        log_error("Controller socket already initialized");
        return -1;
    }

    controller_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (controller_sock < 0) {
        log_perror("Failed to create controller socket");
        return -1;
    }

    // ???: Why set REUSEPORT and REUSEADDR on a socket that's not binding?
    int val = 1;
    if (setsockopt(controller_sock, SOL_SOCKET, SO_REUSEPORT,
                   &val, sizeof(val)) < 0 ) {
        log_perror("Error setting SO_REUSEPORT");
    }
    val = 1;
    if (setsockopt(controller_sock, SOL_SOCKET, SO_REUSEADDR,
                   &val, sizeof(val)) < 0) {
        log_perror("Error setting SO_REUSEADDR");
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
    int port = ntohs(addr->sin_port);
    if (connect(controller_sock, (struct sockaddr*) addr, sizeof(*addr)) < 0) {
        log_perror("Failed to connect to master at %s:%d", ip, port);
        close(controller_sock);
        return -1;
    }

    log_info("Connected to global controller at %s:%d", ip, port);
    return controller_sock;
}

int init_controller_socket(struct sockaddr_in *addr) {
    int sock = connect_to_controller(addr);
    if (sock < 0) {
        log_error("Error connecting to global controller");
        return -1;
    }
    if (monitor_socket(sock) != 0) {
        log_error("Attempted to initialize controller socket "
                  "before initializing runtime epoll");
        return -1;
    }
    return 0;
}

static ssize_t read_ctrl_message(int fd, struct ctrl_runtime_msg_hdr *msg) {
    ssize_t rtn = read(fd, (void*)msg, sizeof(*msg));
    if (rtn != sizeof(*msg)) {
        log_error("Could not read full control runtime message from socket %d", fd);
        return -1;
    }
    return rtn;
}

#define CHECK_MSG_SIZE(msg, target) \
    if (msg->payload_size != sizeof(target)) { \
        log_error("Message data size (%d) does not match size" \
                 "of target type " #target, (int)msg->payload_size ); \
        return -1; \
    } \
    return 0;

static int verify_msg_size(struct ctrl_runtime_msg_hdr *msg) {
    switch (msg->type) {
        case CTRL_ADD_RUNTIME:
            CHECK_MSG_SIZE(msg, struct ctrl_add_runtime_msg);
        case CTRL_CREATE_THREAD:
            CHECK_MSG_SIZE(msg, struct ctrl_create_thread_msg);
        case CTRL_DELETE_THREAD:
            CHECK_MSG_SIZE(msg, struct ctrl_create_thread_msg);
        case CTRL_MODIFY_ROUTE:
            CHECK_MSG_SIZE(msg, struct ctrl_route_mod_msg);
        case CTRL_CREATE_MSU:
            CHECK_MSG_SIZE(msg, struct ctrl_create_msu_msg);
        case CTRL_DELETE_MSU:
            CHECK_MSG_SIZE(msg, struct ctrl_delete_msu_msg);
        case CTRL_MSU_ROUTES:
            CHECK_MSG_SIZE(msg, struct ctrl_msu_route_msg);
        default:
            log_error("Received unknown message type: %d", msg->type);
            return -1;
    }
}

static int read_ctrl_payload(int fd, size_t size, void *buff) {
    ssize_t rtn = read(fd, buff, size);
    if (rtn != size) {
        log_error("Could not read full control payload from socket %d", fd);
        return -1;
    }
    return 0;
}

static enum thread_msg_type get_thread_msg_type(enum ctrl_runtime_msg_type type) {
    switch (type) {
        case CTRL_ADD_RUNTIME:
            return ADD_RUNTIME;
        case CTRL_CREATE_THREAD:
            return CREATE_THREAD;
        case CTRL_DELETE_THREAD:
            return DELETE_THREAD;
        case CTRL_MODIFY_ROUTE:
            return MODIFY_ROUTE;
        case CTRL_CREATE_MSU:
            return CREATE_MSU;
        case CTRL_DELETE_MSU:
            return DELETE_MSU;
        case CTRL_MSU_ROUTES:
            return MSU_ROUTE;
        default:
            log_error("Unknown thread message type %d", type);
            return NIL;
    }
}

static int process_ctrl_message(struct ctrl_runtime_msg_hdr *msg, int fd) {
    if (verify_msg_size(msg) != 0) {
        log_error("Cannot process message. Incorrect payload size for type.");
        return -1;
    }

    void *msg_data = malloc(msg->payload_size);
    int rtn = read_ctrl_payload(fd, msg->payload_size, msg_data);
    if (rtn < 0) {
        log_error("Error reading control payload. Cannot process message");
        return -1;
    }

    enum thread_msg_type type = get_thread_msg_type(msg->type);
    struct thread_msg *thread_msg = construct_thread_msg(
            type, msg->payload_size, msg_data
    );

    struct dedos_thread *thread = get_dedos_thread(msg->thread_id);
    if (thread == NULL) {
        log_error("Error getting dedos thread %d to deliver control message",
                  msg->thread_id);
        return -1;
    }

    rtn = enqueue_thread_msg(thread_msg, &thread->queue);
    if (rtn < 0) {
        log_error("Error enqueuing control message on thread %d", msg->thread_id);
        return -1;
    }
    return 0;
}

int handle_controller_communication(int fd) {
    struct ctrl_runtime_msg_hdr  msg;
    ssize_t msg_size = read_ctrl_message(fd, &msg);
    if (msg_size < 0) {
        log_error("Error reading control message");
    } else {
        log_custom(LOG_CONTROLLER_COMMUNICATION, 
                   "Read message from controller of size %d", msg_size);
    }

    int rtn = process_ctrl_message(&msg, fd);
    if (rtn < 0) {
        log_error("Error processing control message");
        return -1;
    }

    return 0;
}

bool is_controller_fd(int fd) {
    return fd == controller_sock;
}
