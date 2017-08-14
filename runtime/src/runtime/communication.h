#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdbool.h>

enum fd_handler {
    RUNTIME_HANDLER,
    CONTROLLER_HANDLER
};

struct dedos_tcp_msg {
    int src_id;
    size_t data_len;
}

int send_to_endpoint(int fd, void *data, size_t data_len);
int init_bound_socket(int port);
int init_runtime_epoll();
int add_to_runtime_epoll(int fd enum fd_handler handler);
int check_runtime_epoll(bool blocking);

#endif
