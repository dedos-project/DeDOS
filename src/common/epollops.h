#ifndef EPOLL_OPS_H
#define EPOLL_OPS_H
#include <stdint.h>
#include <stdbool.h>

int enable_epoll(int epoll_fd, int new_fd, uint32_t events);

int add_to_epoll(int epoll_fd, int new_fd, uint32_t events, bool oneshot);

int epoll_loop(int socket_fd, int epoll_fd, int batch_size, int timeout,
       bool oneshot,
       int (*connection_handler)(int, void*),
       int (*accept_handler)(int, void*),
       void *data);

int init_epoll(int socket_fd);

#endif
