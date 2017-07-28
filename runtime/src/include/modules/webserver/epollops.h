#ifndef EPOLLOPS_H_
#define EPOLLOPS_H_

int enable_epoll(int epoll_fd, int new_fd, uint32_t events);
int epoll_loop(int socket_fd, int epoll_fd,
       int (*connection_handler)(int, void*), void *data);
int init_epoll(int socket_fd);

#endif
