#include "socket_interface.h"
#include "cli.h"
#include "logging.h"

#include <stdlib.h>
#include <zmq.h>

#define MAX_RECV_LEN 1024

static void* socket_loop(void *v_port) {
    int port = *(int*)v_port;
    free(v_port);
    void *context = zmq_ctx_new();
    if (context == NULL) {
        log_error("Error instantiating ZMQ context");
        return NULL;
    }
    void *socket = zmq_socket(context, ZMQ_REP);
    if (socket == NULL) {
        log_error("Error instantiating ZMQ socket");
        return NULL;
    }

    int rtn;
    if (port <= 0) {
        rtn = zmq_bind(socket, "ipc:///tmp/dedosipc");
        if (rtn != 0) {
            log_perror("Error binding ZMQ socket to ipc");
            return NULL;
        } else {
            log_info("Bound ipc socket");
        }
    } else {
        char port_str[32];
        sprintf(port_str, "tcp://*:%d", port);
        rtn = zmq_bind(socket, port_str);
        if (rtn != 0) {
            log_perror("Error binding ZMQ socket to %s", port_str);
            return NULL;
        } else {
            log_info("Bound control socket: %s", port_str);
        }
    }

    char buf[MAX_RECV_LEN];
    while (1) {
        rtn = zmq_recv(socket, buf, MAX_RECV_LEN, 0);
        if (rtn < 0) {
            log_perror("Error receiving ZMQ message");
            return NULL;
        }
        buf[rtn] = '\0';

        log_info("Received over socket: %s", buf);

        int cmd_rtn = parse_cmd_action(buf);
        char cmd_rtn_c[32];
        sprintf(cmd_rtn_c, "%d", cmd_rtn);

        zmq_send(socket, cmd_rtn_c, strlen(cmd_rtn_c), 0);
    }
}


int start_socket_interface_thread(pthread_t *thread, int port) {
    int *p_port = malloc(sizeof(*p_port));
    *p_port = port;
    int err = pthread_create(thread, NULL, socket_loop, p_port);
    if (err != 0) {
        log_error("ERROR: can't create thread: %s", strerror(err));
    } else {
        log_info("Socket Control Thread created successfully");
    }
    return err;
}
