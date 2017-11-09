/**
 * @file socket_monitor.c
 *
 * Monitors an incoming port for messages from runtime or controller
 */
#include "epollops.h"
#include "communication.h"
#include "logging.h"
#include "controller_communication.h"
#include "runtime_communication.h"

#include <sys/epoll.h>

/** The maximum file descriptor that can be associated with a runtime */
#define MAX_RUNTIME_FD 1024
/** Whether the file descriptor has been associated with a runtime */
static bool runtime_fds[MAX_RUNTIME_FD];

/** The socket on which incoming connections are monitored */
int runtime_socket = -1;
/** The epoll file descriptor monitoring sockets */
int epoll_fd = -1;

/** Initializes the socket monitor on the given port */
static int init_socket_monitor(int port) {
    if (runtime_socket > 0) {
        log_error("Runtime socket already initialized to %d. Cannot reinitialize",
                  runtime_socket);
        return -1;
    }
    runtime_socket = init_runtime_socket(port);
    if (runtime_socket < 0) {
        log_error("Error initializing runtime socket on port %d", port);
        return -1;
    }
    log_info("Initialized runtime socket on port %d", port);

    epoll_fd = init_epoll(runtime_socket);

    if (epoll_fd < 0) {
        log_error("Error initializing runtime epoll. Closing runtime socket.");
        close(runtime_socket);
        return -1;
    }

    return 0;
}

/** Checks if the file descriptor is a runtime or controller, and handles it appropraitely */
static int handle_connection(int fd, void *data) {
    if (runtime_fds[fd]) {
        int rtn = handle_runtime_communication(fd);
        if (rtn != 0) {
            return rtn;
        }
        return 0;
    } else {
        // It's a controller file descriptor. Can add check if necessary,
        // but this epoll should only handle runtimes or controller
        int rtn = handle_controller_communication(fd);
        if (rtn != 0) {
            return rtn;
        }
        return 0;
    }
    return 0;
}

/** Accepts a new connection. Assumes it is a runtime */
static int accept_connection(int fd, void *data) {
    if (fd > MAX_RUNTIME_FD) {
        log_error("Cannot accept runtime connection on file descriptor greater than %d",
                  MAX_RUNTIME_FD);
        // note: returning 0, not -1, because -1 will make epoll loop exit
        return 0;
    }
    runtime_fds[fd] = true;
    return 0;
}

/** The main loop for the socket monitor. Simply calls epoll_loop */
static int socket_monitor_epoll_loop() {
    if (runtime_socket == -1 || epoll_fd == -1) {
        log_error("Runtime socket or epoll not initialized. Cannot start monitor loop");
        return -1;
    }

    int rtn = epoll_loop(runtime_socket, epoll_fd, -1, -1, 0,
                         handle_connection, accept_connection, NULL);
    if (rtn < 0) {
        log_error("Epoll loop exited with error");
    }
    log_info("Epoll loop exited");
    return rtn;
}

int monitor_controller_socket(int new_fd) {
    if ( epoll_fd == -1 ) {
        log_error("Epoll not initialized. Cannot monitor new socket");
        return -1;
    }

    int rtn = add_to_epoll(epoll_fd, new_fd, EPOLLIN, false);
    if (rtn < 0) {
        log_perror("Error adding socket %d to epoll", new_fd);
        return -1;
    }
    return 0;
}

int monitor_runtime_socket(int new_fd) {
    if ( epoll_fd == -1 ) {
        log_error("Epoll not initialized. Cannot monitor new socket");
        return -1;
    }

    int rtn = add_to_epoll(epoll_fd, new_fd, EPOLLIN, false);
    runtime_fds[new_fd] = true;
    if (rtn < 0) {
        log_perror("Error adding socket %d to epoll", new_fd);
        return -1;
    }
    return 0;
}

int run_socket_monitor(int local_port, struct sockaddr_in *ctrl_addr) {
    int rtn = init_socket_monitor(local_port);
    if (rtn < 0) {
        log_critical("Error initializing socket monitor");
        return -1;
    }

    int sock;
    do {
        log_info("Attempting to connect to controller");
        sock = init_controller_socket(ctrl_addr);
        if (sock < 0)
            sleep(1);
    } while (sock <= 0);

    log_info("Connected to global controller on socket %d. Entering socket monitor loop.", sock);

    return socket_monitor_epoll_loop();
}
