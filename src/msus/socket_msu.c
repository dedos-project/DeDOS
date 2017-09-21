#include "socket_msu.h"
#include "local_msu.h"
#include "epollops.h"
#include "logging.h"
#include "msu_message.h"
#include "runtime_dfg.h"
#include "communication.h"

#include <stdlib.h>
#include <netinet/ip.h>

#define MAX_FDS 65536

struct local_msu *instance;

struct sock_msu_state {
    int sock_fd;
    int epoll_fd;
    struct local_msu *self;
    struct msu_type *default_target;
    struct msu_msg_hdr *hdr_mask[MAX_FDS];
    struct local_msu *destinations[MAX_FDS];
};


#define SOCKET_HANDLER_TIMEOUT 100
#define SOCKET_HANDLER_BATCH_SIZE 100

struct key_seed {
    struct sockaddr_in sockaddr;
    uint32_t local_ip;
};

int msu_monitor_fd(int fd, uint32_t events, struct local_msu *destination,
                   struct msu_msg_hdr *hdr) {
    struct sock_msu_state *state = instance->msu_state;

    state->hdr_mask[fd] = hdr;
    state->destinations[fd] = destination;

    int rtn = enable_epoll(state->epoll_fd, fd, events);

    if (rtn < 0) {
        log_error("Error enabling epoll");
        return -1;
    }
    return 0;
}

static int process_connection(int fd, void *v_state) {
    struct sock_msu_state *state = v_state;
    log(LOG_SOCKET_HANDLER, "Processing connection: fd = %d", fd);

    struct socket_msg *msg = malloc(sizeof(*msg));
    msg->fd = fd;

    struct local_msu *destination= state->destinations[fd];
    if (destination == NULL) {
        // This is a file descriptor we haven't seen before
        log(LOG_SOCKET_HANDLER, "New connection: fd = %d", fd);

        // Generate the key for the message
        struct msu_msg_key key;
        struct key_seed seed;
        socklen_t addrlen = sizeof(seed.sockaddr);
        int rtn = getpeername(fd, (struct sockaddr*)&seed.sockaddr, &addrlen);
        if (rtn < 0) {
            log_perror("Could not getpeername() on fd %d", fd);
            return -1;
        }
        seed.local_ip = local_runtime_ip();
        set_msg_key(&seed, sizeof(seed), &key);

        struct msu_type *type = state->default_target;

        rtn = init_call_msu(state->self, type, &key, sizeof(*msg), msg);
        if (rtn < 0) {
            log_error("Error enqueing to destination");
            free(msg);
            return -1;
        }
        return 0;
    } else {
        // This file descriptor was enqueued with a particular target in mind
        struct msu_msg_hdr *hdr = state->hdr_mask[fd];
        if (hdr == NULL) {
            log_error("Existing destination with null header for fd %d", fd);
            return -1;
        }
        int rtn = call_local_msu(state->self, destination, hdr, sizeof(*msg), msg);
        if (rtn < 0) {
            log_error("Error enqueueing to next MSU");
            return -1;
        }
        log(LOG_SOCKET_HANDLER,"Enqueued to MSU %d", destination->id);
        return 0;
    }
}

static int set_default_target(int fd, void *v_state) {
    struct sock_msu_state *state = v_state;
    state->hdr_mask[fd] = NULL;
    state->destinations[fd] = NULL;
    return 0;
}

static int socket_handler_main_loop(struct local_msu *self) {
    struct sock_msu_state *state = self->msu_state;

    int rtn = epoll_loop(state->sock_fd, state->epoll_fd,
                         SOCKET_HANDLER_BATCH_SIZE, SOCKET_HANDLER_TIMEOUT, true,
                         process_connection,
                         set_default_target,
                         state);
    return rtn;
}

struct msu_msg_key self_key = {
    .key = {0},
    .key_len = 0,
    .id = 0
};

static int socket_msu_receive(struct local_msu *self, struct msu_msg *data) {
    int rtn = socket_handler_main_loop(self);
    if (rtn < 0) {
        log_error("Error exiting socket handler main loop");
        return -1;
    }
    init_call_local_msu(self, self, &self_key, 0, NULL);
    return 0;
}


static void socket_msu_destroy(struct local_msu *self) {
    struct sock_msu_state *state = self->msu_state;

    int rtn = close(state->sock_fd);
    if (rtn == -1) {
        log_error("Error closing socket");
    }
}

#define DEFAULT_PORT 8080
#define DEFAULT_TARGET 501
#define INIT_SYNTAX "<port>, <target_msu_type>"

struct sock_init {
    int port;
    int target_type;
};

static int parse_init_payload (char *to_parse, struct sock_init *parsed) {

    parsed->port = DEFAULT_PORT;
    parsed->target_type = DEFAULT_TARGET;

    if (to_parse == NULL) {
        log_warn("Initializing socket handler MSU with default parameters. "
                 "(FYI: init syntax is [" INIT_SYNTAX "])");
    } else {
        char *saveptr, *tok;
        if ( (tok = strtok_r(to_parse, " ,", &saveptr)) == NULL) {
            log_warn("Couldn't get port from initialization string");
            return 0;
        }
        parsed->port = atoi(tok);

        if ( (tok = strtok_r(NULL, " ,", &saveptr)) == NULL) {
            log_warn("Couldn't get target MSU from initialization string");
            return 0;
        }
        parsed->target_type = atoi(tok);

        if ( (tok = strtok_r(NULL, " ,", &saveptr)) != NULL) {
            log_warn("Discarding extra tokens from socket initialzation: %s", tok);
        }
    }
    return 0;
}


static int socket_msu_init(struct local_msu *self, struct msu_init_data *init_data) {

    if (instance != NULL) {
        log_error("Socket MSU already instantiated. There can be only one!");
        return -1;
    }

    char *init_cmd = init_data->init_data;
    struct sock_init init;
    parse_init_payload(init_cmd, &init);

    log(LOG_SOCKET_INIT, "Initializing socket handler with port %d, target %d",
               init.port, init.target_type);

    struct sock_msu_state *state = malloc(sizeof(*state));
    self->msu_state = state;

    state->default_target = get_msu_type(init.target_type);

    if (state->default_target == NULL) {
        log_error("Cannot get type %d for socket handler", init.target_type);
        return -1;
    }

    state->sock_fd = init_listening_socket(init.port);
    if (state->sock_fd == -1) {
        log_error("Couldn't initialize socket for socket handler MSU %d", self->id);
        return -1;
    }

    state->epoll_fd = init_epoll(state->sock_fd);
    if (state->epoll_fd == -1) {
        log_error("Couldn't initialize epoll_Fd for socket handler MSU %d", self->id);
        return -1;
    }

    state->self = self;
    instance = self;

    init_call_local_msu(self, self, &self_key, 0, NULL);

    return 0;
}

struct msu_type SOCKET_MSU_TYPE = {
    .name = "socket_msu",
    .id = SOCKET_MSU_TYPE_ID,
    .init = socket_msu_init,
    .destroy = socket_msu_destroy,
    .receive = socket_msu_receive
};

