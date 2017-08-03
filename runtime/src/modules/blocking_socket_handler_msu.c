/**
 * @file blocking_socket_handler_msu.c
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

#include "modules/webserver/socketops.h"
#include "modules/webserver/epollops.h"
#include "modules/blocking_socket_handler_msu.h"
#include "msu_queue.h"
#include "dedos_msu_list.h"
#include "runtime.h"
#include "modules/webserver_routing_msu.h"
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

#define MAX_FDS 2048
/**
 * A reference to the instance of the socket handler MSU, so epoll can be accessed
 * from outside of the thread
 */
struct generic_msu *socket_handler_instance = NULL;

/**
 * Internal state for socket handling
 */
struct blocking_socket_handler_state {
    int socketfd;
    int epollfd; //descriptor for events
    int target_msu_type;
    int proxy_fd[MAX_FDS];
};

/**
 * Checks if the instance of the socket handler MSU has been initialized
 */
int is_socket_handler_initialized() {
    // TODO: More thorough checks
    return socket_handler_instance != NULL;
}
/**
 * Adds the file descriptor to the epoll instance, without necessarily having the reference
 * to the socket handler MSU itself.
 * To be used from external MSUs.
 */
int monitor_fd(int fd, uint32_t events) {
    if (! is_socket_handler_initialized()) {
        log_warn("Not accepting new connection! No initialized socket handler!");
        return -1;
    }

    struct blocking_socket_handler_state *state =
            socket_handler_instance->internal_state;

    int rtn = enable_epoll(state->epollfd, fd, events);
    state->proxy_fd[fd] = -1;
    if (rtn < 0) {
        log_error("Error enabling epoll for fd %d on socket handler MSU", fd);
        return -1;
    }

    log_custom(LOG_SOCKET_HANDLER, "Enabled epoll for fd %d", fd);
    return 0;
}

int add_proxy_fd(int fd, uint32_t events, int proxy) {
    struct blocking_socket_handler_state *state =
            socket_handler_instance->internal_state;
    int rtn = add_to_epoll(state->epollfd, fd, events);
    if (rtn < 0) {
        rtn = enable_epoll(state->epollfd, fd, events);
    }
    if (rtn < 0) {
        log_error("Error adding to epoll");
        return -1;
    }
    state->proxy_fd[fd] = proxy;
    return 0;
}


static int set_no_proxy(int fd, void *v_state) {
    struct blocking_socket_handler_state *state = v_state;
    state->proxy_fd[fd] = -1;
    return 0;
}

/**
 * Processes incoming data on an existing connection, passing the ready
 * file descriptor to the appropriate next MSU
 */
static int process_existing_connection(int fd, void *v_state) {
    struct blocking_socket_handler_state *state = v_state;
    log_custom(LOG_SOCKET_HANDLER, "Processing existing connection on fd %d", fd);
    if (state->proxy_fd[fd] > 0) {
        fd = state->proxy_fd[fd];
    }
    switch (state->target_msu_type) {

        case DEDOS_WEBSERVER_ROUTING_MSU_ID:;
            struct generic_msu *ws_routing = webserver_routing_instance();
            if (ws_routing == NULL) {
                log_error("ssl_request_routing_msu is NULL! Forgot to create it?");
                // TODO: Handle errors
                return -1;
            } else {
                struct webserver_queue_data *data = malloc(sizeof(*data));
                data->fd = fd;
                data->ip_address = runtimeIpAddress;

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
                rtn = initial_msu_enqueue((void*)data, sizeof(*data), id, ws_routing);
                if (rtn != 0) {
                    log_error("Error enqueuing request %d (fd: %d) to ssl_routing", id, fd);
                    return -1;
                }
                log_custom(LOG_SOCKET_HANDLER,
                    "Enqueued request %d (fd: %d) to ssl_request_routing_msu", id, fd);
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
    struct blocking_socket_handler_state *state = self->internal_state;

    int rtn = epoll_loop(state->socketfd, state->epollfd,
                         process_existing_connection,
                         set_no_proxy, (void*)state);
    return rtn;
}

/**
 * The main receive function for the socket handler.
 * If the file descriptor received in the data payload is -1, a call to this function
 * kicks off the socket handler's main epoll loop.
 * If the file descriptor received is > 0, a call to this function adds the fd to the
 * socket handler's file descriptor set
 */
static int socket_handler_receive(struct generic_msu *self, struct generic_msu_queue_item *data) {
    // TODO: Right now this call blocks forever
    // This should be fixed so it occasionally exits out to the main thread loop
    // in case of messages or deletion
    int rtn = socket_handler_main_loop(self);
    if (rtn < 0) {
        log_error("Error exiting socket handler main loop");
        return -1;
    }
    // So that the message will get enqueued to this MSU again
    return DEDOS_BLOCKING_SOCKET_HANDLER_MSU_ID;
}

struct blocking_sock_init {
    struct sock_settings sock_settings;
    int target_msu_type;
};

#define DEFAULT_PORT 8080
#define DEFAULT_TARGET 501
#define INIT_SYNTAX "<port>, <target_msu_type>"

static int parse_init_payload (char *to_parse, struct blocking_sock_init *parsed) {

    parsed->sock_settings = *webserver_sock_settings(DEFAULT_PORT);
    parsed->target_msu_type = DEFAULT_TARGET;

    if (to_parse == NULL) {
        log_warn("Initializing socket handler MSU with default parameters. "
                 "(FYI: init syntax is [" INIT_SYNTAX "])");
    } else {
        char *saveptr, *tok;
        if ( (tok = strtok_r(to_parse, " ,", &saveptr)) == NULL) {
            log_warn("Couldn't get port from initialization string");
            return 0;
        }
        parsed->sock_settings.port = atoi(tok);

        if ( (tok = strtok_r(NULL, " ,", &saveptr)) == NULL) {
            log_warn("Couldn't get target MSU from initialization string");
            return 0;
        }
        parsed->target_msu_type = atoi(tok);

        if ( (tok = strtok_r(NULL, " ,", &saveptr)) != NULL) {
            log_warn("Discarding extra tokens from socket initialzation: %s", tok);
        }
    }
    return 0;
}

#define SOCKET_BACKLOG 512
/**
 * Init an event structure for epolling
 * @param struct generic_msu *self: pointer to the msu instance
 * @param struct create_msu_thread_data *initial_state: contains init data
 * @return 0/-1 success/failure
 */
static int socket_handler_init(struct generic_msu *self, struct create_msu_thread_data *initial_state) {
    if (socket_handler_instance != NULL) {
        log_error("Socket handler already instantiated! There can only be one.");
        return -1;
    }
    char *init_cmd= initial_state->init_data;

    struct blocking_sock_init init;
    parse_init_payload(init_cmd, &init);
    log_info("Initializing socket handler with port: %d, target_msu: %d",
             init.sock_settings.port, init.target_msu_type);


    struct blocking_socket_handler_state *state = malloc(sizeof(*state));
    self->internal_state = state;
    state->target_msu_type = init.target_msu_type;

    state->socketfd = init_socket(&init.sock_settings);
    if ( state->socketfd == -1) {
        log_error("Couldn't initialize socket for socket handler MSU %d", self->id);
        return -1;
    }

    state->epollfd = init_epoll(state->socketfd);
    if (state->epollfd == -1) {
        log_error("Couldn't initialize epoll_fd for socket handler MSU %d", self->id);
        return -1;
    }

    // Assign the global instance of the socket handler to be this MSU
    socket_handler_instance = self;

    // Finally we have to make sure that the main loop runs. We can do this by enqueing
    // a queue item with a NULL payload to this MSU
    initial_msu_enqueue(NULL, 0, 0, self);

    return 0;
}

/**
 * Close sockets handled by this MSU
 * @param: struct generic_msu self reference to this MSU
 * @param create_msu_thread_data initial_state reference to initial data structure
 * @return 0/-1 success/failure
 */
static void socket_handler_destroy(struct generic_msu *self) {
    struct blocking_socket_handler_state *state = self->internal_state;
    int ret;

    ret = close(state->socketfd);
    if (ret == - 1) {
        log_error("%s", "error closing socket");
    }
}

/**
 * Keep track of a set of file descriptors and poll them
 */
struct msu_type BLOCKING_SOCKET_HANDLER_MSU_TYPE = {
    .name="blocking_socket_handler_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_BLOCKING_SOCKET_HANDLER_MSU_ID,
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
