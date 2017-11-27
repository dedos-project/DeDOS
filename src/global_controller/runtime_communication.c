/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania

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
#include "runtime_communication.h"
#include "communication.h"
#include "logging.h"
#include "rt_controller_messages.h"
#include "stat_msg_handler.h"
#include "epollops.h"
#include "stats.h"
#include "unused_def.h"
#include "dfg_writer.h"
#include "haproxy.h"

#include <signal.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/stat.h>

struct runtime_endpoint {
    int fd;
    uint32_t ip;
    int port;
};

#define MAX_RUNTIME_ID 32

static struct runtime_endpoint runtime_endpoints[MAX_RUNTIME_ID];

int runtime_fd(unsigned int runtime_id) {
    if (runtime_endpoints[runtime_id].fd > 0) {
        return runtime_endpoints[runtime_id].fd;
    }
    return 0;
}

static int runtime_id(int runtime_fd) {
    for (int i=0; i < MAX_RUNTIME_ID; i++) {
        if (runtime_endpoints[i].fd == runtime_fd) {
            return i;
        }
    }
    return -1;
}

int send_to_runtime(unsigned int runtime_id, struct ctrl_runtime_msg_hdr *hdr, void *payload) {
    if (runtime_id > MAX_RUNTIME_ID) {
        log_error("Requested runtime %d is greater than max runtime ID %d",
                  runtime_id, MAX_RUNTIME_ID);
        return -1;
    }
    struct runtime_endpoint *endpoint = &runtime_endpoints[runtime_id];
    if (endpoint->fd <= 0) {
        log_error("Requested runtime %d not instantiated (fd: %d)", runtime_id, endpoint->fd);
        return -1;
    }

    int rtn = send_to_endpoint(endpoint->fd, hdr, sizeof(*hdr));
    if (rtn <= 0) {
        log_error("Error sending header to runtime %d", runtime_id);
        return -1;
    }

    if (hdr->payload_size <= 0) {
        return 0;
    }

    rtn = send_to_endpoint(endpoint->fd, payload, hdr->payload_size);
    if (rtn <= 0) {
        log_error("Error sending payload to runtime %d", runtime_id);
        return -1;
    }
    log(LOG_RUNTIME_SENDS, "Sent a payload of size %d to runtime %d (fd: %d)",
               (int)hdr->payload_size, runtime_id, endpoint->fd);
    return 0;
}

static int send_add_runtime_msg(unsigned int target_id, int new_rt_id,
                                uint32_t ip, int port) {
    struct ctrl_add_runtime_msg msg = {
        .runtime_id = new_rt_id,
        .ip = ip,
        .port = port
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_CONNECT_TO_RUNTIME,
        .thread_id = 0,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_runtime(target_id, &hdr, &msg);
    if (rtn < 0) {
        log_error("Error sending initialization message for rt %d to rt %d",
                  new_rt_id, target_id);
    } else {
        log(LOG_RT_COMMUNICATION, "Send initialization message for rt %d to rt %d",
                   new_rt_id, target_id);
    }
    return 0;
}

static int remove_runtime_endpoint(int fd) {
    for (int i=0; i<MAX_RUNTIME_ID; i++) {
        if (runtime_endpoints[i].fd == fd) {
            close(runtime_endpoints[i].fd);
            runtime_endpoints[i].fd = 0;
            return i;
        }
    }
    return -1;
}

static int add_runtime_endpoint(unsigned int runtime_id, int fd, uint32_t ip, int port) {
    struct stat buf;
    if (fstat(fd, &buf) != 0) {
        log_error("Cannot register non-descriptor %d for runtime ID %d", fd, runtime_id);
        return -1;
    }
    if (runtime_endpoints[runtime_id].fd != 0) {
        log_warn("Replacing runtime peer with id %d", runtime_id);
    }
    runtime_endpoints[runtime_id].fd = fd;

    int n_sent = 0;
    for (int i=0; i<MAX_RUNTIME_ID; i++) {
        if (i != runtime_id && runtime_endpoints[i].fd != 0) {
            if (send_add_runtime_msg(i, runtime_id, ip, port) != 0) {
                log_error("Failed to add runtime %d to runtime %d (fd: %d)",
                          runtime_id, i, runtime_endpoints[runtime_id].fd);
            } else {
                n_sent++;
            }
        }
    }
    set_haproxy_weights(0,0);
    return 0;
}

static int process_rt_init_message(ssize_t payload_size, int fd) {
    struct rt_controller_init_msg msg;
    if (payload_size != sizeof(msg)) {
        log_error("Payload size does not match size of runtime initialization message");
        return -1;
    }
    int rtn = read_payload(fd, sizeof(msg), &msg);
    if (rtn < 0) {
        log_error("Error reading runtime init payload. Cannot process message");
        return -1;
    }

    rtn = add_runtime_endpoint(msg.runtime_id, fd, msg.ip, msg.port);
    if (rtn < 0) {
        log_error("Error adding runtime endpoint");
        return -1;
    }
    log_info("Added runtime endpoint %d (fd: %d)", msg.runtime_id, fd);
    return 0;
}

static int process_rt_stats_message(size_t payload_size, int fd) {

    char buffer[payload_size];
    int rtn = read_payload(fd, sizeof(buffer), &buffer);
    if (rtn < 0) {
        log_error("Error reading stats message payload");
        return -1;
    }
    int id = runtime_id(fd);
    if (id < 0) {
        log_error("Cannot get runtime ID from file descriptor");
        return -1;
    }
    return handle_serialized_stats_buffer(id, buffer, payload_size);
}

static int process_rt_message_hdr(struct rt_controller_msg_hdr *hdr, int fd) {
    int rtn;
    switch (hdr->type) {
        case RT_CTL_INIT:
            rtn = process_rt_init_message(hdr->payload_size, fd);
            if (rtn < 0) {
                log_error("Erorr processing init runtime message from fd %d", fd);
                return -1;
            }
            return 0;
        case RT_STATS:
            rtn = process_rt_stats_message(hdr->payload_size, fd);
            if (rtn < 0) {
                log_error("Error processing rt stats message from fd %d", fd);
                return -1;
            }
            return 0;
        default:
            log_error("Received unknown message type from fd %d: %d", fd, hdr->type);
            return -1;
    }
}

#define MAX_OUTPUT_SOCKS 2

static int output_listen_sock = -1;
static int output_socks[MAX_OUTPUT_SOCKS];
static int output_sock_idx = 0;

static void add_output_sock(int fd) {
    if (output_socks[output_sock_idx] > 0) {
        close(output_socks[output_sock_idx]);
    }
    output_socks[output_sock_idx] = fd;
    output_sock_idx++;
    output_sock_idx %= MAX_OUTPUT_SOCKS;
}


static int handle_runtime_communication(int fd, void UNUSED *data) {
    struct rt_controller_msg_hdr hdr;

    if (fd == output_listen_sock) {
        log_info("Adding new output port");
        int new_fd = accept(fd, NULL, 0);
        if (new_fd > 0) {
            add_output_sock(new_fd);
            log_info("Added new output listener");
            return 0;
        } else {
            log_error("Error adding new listener");
            return 0;
        }
    }

    int rtn = read_payload(fd, sizeof(hdr), &hdr);
    if (rtn < 0) {
        log_error("Error reading runtime message header from fd %d", fd);
        return 1;
    } else if (rtn == 1) {
        int id = remove_runtime_endpoint(fd);
        if (id < 0) {
            log_error("Error shutting down socket %d", fd);
        } else {
            log_info("Runtime %d has been shut down by peer. Removing", id);
        }
        return 0;
    } else {
        log(LOG_RT_COMMUNICATION,
                   "received header from runtime");
    }

    rtn = process_rt_message_hdr(&hdr, fd);
    if (rtn < 0) {
        log_error("Error processing rt message");
        return 0;
    }
    return 0;
}

int get_output_listener(int port) {
    int listen_sock = init_listening_socket(port);
    if (listen_sock < 0) {
        log_error("Error initializing listening socket on port %d", port);
        return -1;
    }

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(listen_sock, &fdset);

    int rtn = -1;
    do {
        log_info("Waiting for DFG reader to connect on port %d!", port);
        struct timeval timeout = {.tv_sec = 1};
        rtn = select(1, &fdset, NULL, NULL, &timeout);
    } while (rtn < 1);

    int output_sock = accept(listen_sock, NULL, 0);
    if (output_sock < 0) {
        log_error("Error accepting output listener!");
        return -1;
    }
    close(listen_sock);
    return output_sock;
}

static int listen_sock = -1;

int runtime_communication_loop(int listen_port, char *output_file, int output_port) {

    signal(SIGPIPE, SIG_IGN);

    if (listen_sock > 0) {
        log_error("Communication loop already started");
        return -1;
    }
    listen_sock = init_listening_socket(listen_port);
    if (listen_sock < 0) {
        log_error("Error initializing listening socket on port %d", listen_port);
        return -1;
    }
    log_info("Starting listening for runtimes on port %d", listen_port);

    int epoll_fd = init_epoll(listen_sock);

    if (epoll_fd < 0) {
        log_error("Error initializing controller epoll. Closing socket");
        close(listen_sock);
        return -1;
    }

    if (output_port > 0) {
        output_listen_sock = init_listening_socket(output_port);
        if (output_listen_sock < 0) {
            log_error("Error listening on port %d", output_port);
            return -1;
        }
        log_info("Listening for DFG reader on port %d", output_port);
        add_to_epoll(epoll_fd, output_listen_sock, EPOLLIN, false);
    }

    struct timespec begin;
    clock_gettime(CLOCK_REALTIME_COARSE, &begin);

    struct timespec elapsed;
    int rtn = 0;
    while (rtn == 0) {
        rtn = epoll_loop(listen_sock, epoll_fd, 1, 1000, 0,
                         handle_runtime_communication, NULL, NULL);
        if (rtn < 0) {
            log_error("Epoll loop exited with error");
            return -1;
        }
        clock_gettime(CLOCK_REALTIME_COARSE, &elapsed);
        if (((elapsed.tv_sec - begin.tv_sec) * 1000 + (elapsed.tv_nsec - begin.tv_nsec) * 1e-6) > STAT_SAMPLE_PERIOD_MS) {
            if (output_file != NULL) {
                dfg_to_file(output_file);
            }
            for (int i=0; i < MAX_OUTPUT_SOCKS; i++) {
                if (output_socks[i] > 0) {
                    if (dfg_to_fd(output_socks[i]) < 0) {
                        close(output_socks[i]);
                        output_socks[i] = 0;
                    }
                }
            }
            clock_gettime(CLOCK_REALTIME_COARSE, &begin);
        }
    }
    log_info("Epoll loop exited");
    return 0;
}
