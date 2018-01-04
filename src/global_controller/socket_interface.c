/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
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
