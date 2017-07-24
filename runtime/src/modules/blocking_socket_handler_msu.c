/**
 * @file socket_handler_msu.c
 * MSU to handle network sockets
 */

// These guards around the headers are necessary if the module
// is to be implemented in c++ instead of C
#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>

#include "modules/blocking_socket_handler_msu.h"
#include "msu_queue.h"
#include "dedos_msu_list.h"
#include "runtime.h"
#include "modules/ssl_request_routing_msu.h"
#include "modules/ssl_msu.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h"
#include "control_protocol.h"
#include "logging.h"

// Enable this #define (as well as LOG_CUSTOM) in the Makefile
// to turn on socket_handler logging
#ifndef LOG_SOCKET_HANDLER
#define LOG_SOCKET_HANDLER 0
#endif

#define GET_NAME_INFO

#ifdef __cplusplus
}
#endif

/**
 * Total number of events that can be reported from epoll at one time
 */
#define MAX_EPOLL_EVENTS 64

/**
 * Internal state for socket handling
 */
struct blocking_socket_handler_state {
    int socketfd;
    int epollfd; //descriptor for events
    int target_msu_type;
};

/**
 * Adds a file descriptor to the epoll instance so it can be responded to
 * at a future time
 */
static inline int add_to_epoll(int epoll_fd, int new_fd) {
    struct epoll_event event;
    event.data.fd = new_fd;
    event.events = EPOLLIN | EPOLLONESHOT;

    int rtn = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &event);

    if (rtn == -1) {
        log_perror("epoll_ctl() failed adding fd %d", new_fd);
        // TODO: Handle errors...
        return -1;
    }
    log_custom(LOG_SOCKET_HANDLER, "Added fd %d to epoll", new_fd);
    return 0;
}

/**
 * Accepts a new connection and adds it to the epoll instance
 */
static int accept_new_connection(int socketfd, int epoll_fd) {
    int rtn;
    log_custom(LOG_SOCKET_HANDLER, "Accepting a new connection");

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int new_fd = accept(socketfd, (struct sockaddr*) &client_addr, &addr_len);
    if (new_fd == -1) {
        log_perror("Error accepting new connection from socket handler");
        return -1;
    }

#ifdef GET_NAME_INFO
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    rtn = getnameinfo((struct sockaddr*)&client_addr, addr_len,
                      hbuf, sizeof(hbuf),
                      sbuf, sizeof(sbuf),
                      NI_NUMERICHOST| NI_NUMERICSERV);
    if ( rtn == 0) {
        log_custom(LOG_SOCKET_HANDLER, "Accepted connection on descriptor %d"
                                       "host=%s, port=%s", 
                   new_fd, hbuf, sbuf);
    }
#endif

    int flags = O_NONBLOCK;
    rtn = fcntl(new_fd, F_SETFL, flags);
    if (rtn == -1) {
        log_perror("Error setting O_NONBLOCK");
        // TODO: Error handling :?
        return -1;
    }
    return add_to_epoll(epoll_fd, new_fd);
}

/** 
 * Processes incoming data on an existing connection, passing the ready
 * file descriptor to the appropriate next MSU
 */
static int process_existing_connection(struct blocking_socket_handler_state *state, int fd) {
    log_custom(LOG_SOCKET_HANDLER, "Processing existing connection on fd %d", fd);
    switch (state->target_msu_type) {

        case DEDOS_SSL_REQUEST_ROUTING_MSU_ID: 
            if (ssl_request_routing_msu == NULL) {
                log_error("ssl_request_routing_msu is NULL! Forgot to create it?");
                // TODO: Handle errors
                return -1;
            } else {
                struct ssl_data_payload *data = malloc(sizeof(*data));
                memset(data, 0, MAX_REQUEST_LEN);
                data->socketfd = fd;
                data->type = READ;
                data->state = NULL;

                // Get the ID for the request based on the client IP and port
                struct sockaddr_in sockaddr;
                socklen_t addrlen = sizeof(sockaddr);
                int rtn = getpeername(fd, (struct sockaddr*)&sockaddr, &addrlen);
                if (rtn < 0) {
                    log_perror("Could not getpeername() for packet ID");
                    return -1;
                }
                uint32_t id;
                HASH_VALUE(&sockaddr, addrlen, id);
                rtn = initial_msu_enqueue((void*)data, sizeof(*data),
                                              id, ssl_request_routing_msu);
                if (rtn != 0) {
                    log_error("Error enqueuing request %d (fd: %d) to ssl_routing",
                              id, fd);
                    return -1;
                }
                log_custom(LOG_SOCKET_HANDLER, 
                    "Enqueued request %d (fd: %d) to ssl_request_routing_msu",
                    id, fd);
                return 0;
            }
        default:
            log_error("Unknown target MSU type (%d) for socket handler MSU",
                      state->target_msu_type);
            return -1;
    }
}

/**
 * The main (blocking) loop for the socket handler.
 * Loops (epolling) indefinitely, accepting new connections and passing new
 * file descriptors to the next MSUs once data is available to be read
 */
static int socket_handler_main_loop(struct generic_msu *self) {
    struct epoll_event events[MAX_EPOLL_EVENTS];
    struct blocking_socket_handler_state *state = self->internal_state;
    while (1) {
        // Waiti indefinitely
        int n = epoll_wait(state->epollfd, events, MAX_EPOLL_EVENTS, -1);

        for (int i = 0; i < n; ++i) {
            if (state->socketfd == events[i].data.fd) {
                if (accept_new_connection(state->socketfd, state->epollfd) != 0) {
                    log_error("Failed accepting new connection");
                } else {
                    log_custom(LOG_SOCKET_HANDLER, "Accepted new connection");
                }
            } else {
                if (process_existing_connection(state, events[i].data.fd) != 0) {
                    log_error("Failed processing existing connection");
                } else { 
                    log_custom(LOG_SOCKET_HANDLER, "Processed existing connection");
                }
            }
        }
    }
    return 0;
}

/**
 * The main receive function for the socket handler.
 * If the file descriptor received in the data payload is -1, a call to this function
 * kicks off the socket handler's main epoll loop.
 * If the file descriptor received is > 0, a call to this function adds the fd to the 
 * socket handler's file descriptor set
 */
static int socket_handler_receive(struct generic_msu *self, struct generic_msu_queue_item *data) {
    if (data->buffer_len != sizeof(struct blocking_socket_handler_data_payload)) {
        log_error("Socket handler received buffer of incorrect size!");
        return -1;
    }

    struct blocking_socket_handler_data_payload *payload = data->buffer;
    if (payload->fd == -1) {
        log_info("Entering socket handler main loop!");
        int rtn = socket_handler_main_loop(self);
        log_info("Exited socket handler main loop!");
        return rtn;
    } else if (payload->fd > 0) {
        struct blocking_socket_handler_state *state = self->internal_state;
        int rtn = add_to_epoll(state->epollfd, payload->fd);
        if (rtn < 0) {
            log_error("Error adding fd %d to epoll!", payload->fd);
            return -1;
        }
        log_custom(LOG_SOCKET_HANDLER, "Added fd %d", payload->fd);
        return 0;
    } else {
        log_error("Improper fd received by socket handler: %d", payload->fd);
        return -1;
    }
}


#define SOCKET_BACKLOG 512
/**
 * Init an event structure for epolling
 * @param struct generic_msu *self: pointer to the msu instance
 * @param struct create_msu_thread_data *initial_state: contains init data
 * @return 0/-1 success/failure
 */
static int socket_handler_init(struct generic_msu *self, struct create_msu_thread_data *initial_state) {
    struct blocking_socket_handler_init_payload *init_data = initial_state->init_data;

    struct blocking_socket_handler_state *state = malloc(sizeof(*state));
    self->internal_state = state;

    state->target_msu_type = init_data->target_msu_type;

    state->socketfd = socket(init_data->domain, init_data->type, init_data->protocol);

    int opt = 1;
    if (setsockopt(state->socketfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        log_perror("setsockopt() failed");
    }

    struct sockaddr_in addr;
    addr.sin_family = init_data->domain;
    addr.sin_addr.s_addr = init_data->bind_ip;
    addr.sin_port = htons(init_data->port);

    opt = 1;
    if (setsockopt(state->socketfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(int)) == -1) {
        log_perror("setsockopt() failed");
        return -1;
    }

    if (bind(state->socketfd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        log_perror("bind() failed");
        return -1;
    }

    if (listen(state->socketfd, SOCKET_BACKLOG) == -1) {
        log_perror("listen() failed");
        return -1;
    }

    state->epollfd = epoll_create1(0);
    if (state->epollfd == -1) {
        log_perror("epoll_create1() failed");
        return -1;
    }

    struct epoll_event event;
    event.data.fd = state->socketfd;
    event.events = EPOLLIN;

    if (epoll_ctl(state->epollfd, EPOLL_CTL_ADD, state->socketfd, &event) == -1) {
        log_perror("epoll_ctl() failed");
        return -1;
    }

    // Finally we have to make sure that the main loop runs, by constructing a payload with a
    // file descriptor of -1 so that msu_receive is called
    struct blocking_socket_handler_data_payload *payload = malloc(sizeof(*payload));
    payload->fd = -1;
    initial_msu_enqueue((void*)payload, sizeof(*payload), 0, self);
    return 0;
}



/**
 * Close sockets handled by this MSU
 * @param: struct generic_msu self reference to this MSU
 * @param create_msu_thread_data initial_state reference to initial data structure
 * @return 0/-1 success/failure
 */
static int socket_handler_destroy(struct generic_msu *self) {
    struct blocking_socket_handler_state *state = self->internal_state;
    int ret;

    ret = close(state->socketfd);
    if (ret == - 1) {
        log_error("%s", "error closing socket");
        return ret;
    }

    return ret;
}

/**
 * Keep track of a set of file descriptors and poll them
 */
struct msu_type BLOCKING_SOCKET_HANDLER_MSU_TYPE = {
    .name="socket_handler_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_SOCKET_HANDLER_MSU_ID,
    .proto_number=0,
    .init=socket_handler_init,
    .destroy=socket_handler_destroy,
    .receive=socket_handler_receive,
    .receive_ctrl=NULL,
    .route=shortest_queue_route,
    .send_local=default_send_local,
    .send_remote=default_send_remote,
    .deserialize=default_deserialize
};
