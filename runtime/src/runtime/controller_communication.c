/**
 * @file: controller_communications.c
 * All communiction with the global controller
 */
#include "controller_communications.h"

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
int send_to_controller(struct control_msg *msg) {
    // TODO: Defaine dedos_control_msg
    size_t buf_len = sizeof(*msg) + msg->payload_len;
    char buf[buf_len];
    memcpy(buf, msg, sizeof(*msg));
    memcpy(buf + sizeof(*msg), msg->payload, msg->payload_len);
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
int connect_to_global_controller(struct sockaddr_in *addr) {
   
    if (controller_socket != -1) {
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
    inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
    int port = noths(addr.sin_port);
    if (connect(controller_sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        log_perror("Failed to connect to master at %s:%d", ip, port);
        close(controller_sock);
        return -1;
    }

    log_info("Connected to global controller at %s:%d", ip, port);
    return controller_sock;
}
             
int init_controller_socket(struct sockaddr_in *addr) {
    int sock = connect_to_global_controller(addr);
    if (sock < 0) {
        log_error("Error connecting to global controller");
        return -1;
    }
    if (add_to_runtime_epoll(sock) != 0) {
        log_error("Attempted to initialize controller socket" 
                  " before initializing runtime epoll");
        return -1;
    }
    return 0;
}
