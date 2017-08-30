#include "epollops.h"
#include "communication.h"
#include "logging.h"
#include "controller_communication.h"
#include "runtime_communication.h"

#include <sys/epoll.h>

enum fd_handler {
    NO_HANDLER,
    RUNTIME_HANDLER,
    CONTROLLER_HANDLER
};

#define MAX_EPOLL_FD 32
static enum fd_handler fd_handlers[MAX_EPOLL_FD];

int runtime_socket = -1;
int epoll_fd = -1;

// TODO: Initialize epoll and runtime socket separately
int init_socket_monitor(int port) {
    if (runtime_socket > 0) {
        log_error("Runtime socket already initialized to %d. Cannot reinitialize",
                  runtime_socket);
        return -1;
    }
    runtime_socket = init_bound_socket(port);
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

    memset(fd_handlers, 0, sizeof(fd_handlers));

    return 0;
}
/*
static void remove_runtime_connection_handler(int fd) {
    fd_handlers[fd] = NO_HANDLER;
}
*/
static int handle_connection(int fd, void *data) {
    int rtn;
    switch (fd_handlers[fd]) {
        case RUNTIME_HANDLER:
            rtn = handle_runtime_communication(fd);
            if (rtn != 0) {
                return rtn;
            }
            break;
        case CONTROLLER_HANDLER:
            rtn = handle_controller_communication(fd);
            if (rtn != 0) {
                return rtn;
            }
            break;
        default:
            log_error("Undefined handler for fd %d", fd);
            return -1;
    }
    return 0;
}

static int accept_connection(int fd, void *data) {
    if (fd > MAX_EPOLL_FD) {
        log_error("Cannot accept runtime connection on file descriptor greater than %d",
                  MAX_EPOLL_FD);
        // NOTE: returning 0, not -1, because -1 will make epoll loop exit
        return 0;
    }
    if (is_runtime_fd(fd)) {
        fd_handlers[fd] = RUNTIME_HANDLER;
    } else if (is_controller_fd(fd)) {
        fd_handlers[fd] = CONTROLLER_HANDLER;
    } else {
        fd_handlers[fd] = NO_HANDLER;
        log_error("Received connection on fd %d, which is neither controller nor runtime!",
                  fd);
    }
    return 0;
}

int socket_monitor_loop() {
    if (runtime_socket == -1 || epoll_fd == -1) {
        log_error("Runtime socket or epoll not initialized. Cannot start monitor loop");
        return -1;
    }

    int rtn = epoll_loop(runtime_socket, epoll_fd, -1, 0, 0,
                         handle_connection, accept_connection, NULL);
    if (rtn < 0) {
        log_error("Epoll loop exited with error");
    }
    log_info("Epoll loop exited");
    return rtn;
}

int monitor_socket(int new_fd) {
    if ( epoll_fd == -1 ) {
        log_error("Epoll not initialized. Cannot monitor new socket");
        return -1;
    }

    int rtn = add_to_epoll(epoll_fd, new_fd, EPOLLIN, false);
    if (rtn < 0) {
        log_error("Error adding socket %d to epoll", new_fd);
        return -1;
    }
    return 0;
}
