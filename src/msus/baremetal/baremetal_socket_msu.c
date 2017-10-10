#include "baremetal/baremetal_socket_msu.h"
#include "baremetal/baremetal_msu.h"
#include "local_msu.h"
#include "epollops.h"
#include "logging.h"
#include "msu_message.h"
#include "runtime_dfg.h"
#include "communication.h"
#include "msu_calls.h"
#include "unused_def.h"

#include <stdlib.h>

#define MAX_RECV_BUF_LEN 64

static struct local_msu *static_self;
static int static_n_hops;
static int static_sock_fd;
static int static_epoll_fd;

static int read_and_forward(int fd, void UNUSED *v_state) {
    char buf[MAX_RECV_BUF_LEN];
    int rtn = read(fd, buf, MAX_RECV_BUF_LEN);
    if (rtn < 0) {
        log_error("Error reading from fd %d!", fd);
        return -1;
    }
    int id = atoi(buf);

    struct msu_msg_key key;

    rtn = set_msg_key(id, &key);
    if (rtn < 0) {
        log_error("Error setting message key to %d", id);
        return -1;
    }

    struct baremetal_msg *msg = malloc(sizeof(*msg));
    msg->fd = fd;
    msg->n_hops = static_n_hops;

    rtn = init_call_msu_type(static_self, &BAREMETAL_MSU_TYPE, &key, sizeof(*msg), msg);
    if (rtn < 0) {
        log_error("Error enqueing to baremetal MSU");
        return -1;
    }
    return 0;
}

#define BAREMETAL_EPOLL_TIMEOUT 1000
#define BAREMETAL_EPOLL_BATCH 1000

static int socket_handler_main_loop(struct local_msu *self) {
    int rtn = epoll_loop(static_sock_fd, static_epoll_fd,
                         BAREMETAL_EPOLL_BATCH, BAREMETAL_EPOLL_TIMEOUT, true,
                         read_and_forward,
                         NULL,
                         NULL);
    return rtn;
}

static struct msu_msg_key self_key = {};

static int bm_sock_receive(struct local_msu *self, struct msu_msg *data) {
    int rtn = socket_handler_main_loop(self);
    if (rtn < 0) {
        log_error("Error exiting socket handler main loop");
        return -1;
    }
    init_call_local_msu(self, self, &self_key, 0, NULL);
    return 0;
}

#define INIT_SYNTAX "<PORT> <N_HOPS>"

static int bm_sock_init(struct local_msu *self, struct msu_init_data *init_data) {

    if (static_self != NULL) {
        log_error("Baremental Socket MSU already instantiated. There can be only one!");
        return -1;
    }
    char *init = init_data->init_data;

    char *str, *saveptr;
    if ((str = strtok_r(init, " ", &saveptr)) == NULL) {
        log_error("Baremetal socket must be initialized with syntax: " INIT_SYNTAX);
        return -1;
    }
    int port = atoi(str);

    if ((str = strtok_r(NULL, " ", &saveptr)) == NULL) {
        log_error("Baremetal socket must be initialized with syntax: " INIT_SYNTAX);
        return -1;
    }
    static_n_hops = atoi(str);

    static_sock_fd = init_listening_socket(port);
    if (static_sock_fd == -1) {
        log_error("Couldn't initialize socket for baremetal sock MSU %d", self->id);
        return -1;
    }

    static_epoll_fd = init_epoll(static_sock_fd);
    if (static_epoll_fd == -1) {
        log_error("Couldn't initialize epoll_Fd for baremetal sock MSU %d", self->id);
        return -1;
    }

    static_self = self;

    init_call_local_msu(self, self, &self_key, 0, NULL);

    return 0;
}

struct msu_type BAREMETAL_SOCK_MSU_TYPE = {
    .name = "baremetal_sock_msu",
    .id = BAREMETAL_SOCK_MSU_TYPE_ID,
    .init = bm_sock_init,
    .receive = bm_sock_receive
};

