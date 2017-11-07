/**
 * @file epollops.h
 *
 * Wrapper functions for epoll to manage event-based communication
 */

#ifndef EPOLL_OPS_H
#define EPOLL_OPS_H
#include <stdint.h>
#include <stdbool.h>

/**
 * Enables a file descriptor which has already been aded to an epoll instance.
 * Or's EPOLLONESHOT with events so the event will only be responded to once
 * @param epoll_fd Epoll file descriptor
 * @param new_fd File descriptor to enable in the epoll instance
 * @param events EPOLLIN &/| EPOLLOUT
 * @return 0 on success, -1 on error
 */
int enable_epoll(int epoll_fd, int new_fd, uint32_t events);

/**
 * Adds a file descriptor to an epoll instance.
 * @param epoll_fd Epoll file descriptor
 * @param new_fd File descriptor to add to the epoll instance
 * @param events EPOLLIN &/| EPOLLOUT
 * @param oneshot Whether to enable EPOLLONESHOT on the added fd
 * @return 0 on success, -1 on error
 */
int add_to_epoll(int epoll_fd, int new_fd, uint32_t events, bool oneshot);

/**
 * The event-based loop for epoll_wait.
 * Loops (epolling), and accepts new connections when they are available on socket_fd.
 * Calls the provided functions on socket activity (connection_handler),
 * and on new connections (accept_handler).
 * NOTE: On first connect, only calls `connection_handler` on EPOLLIN activity.
 * @param socket_fd Socket on which to accept new connections, or -1 if no accept is necessary
 * @param epoll_fd Fd of epoll instance
 * @param batch_size Number of connections to process in a row before exiting the loop or -1 for NA
 * @param timeout Epoll timeout. -1 for no timeout.
 * @param connection_handler Calls this function with args (int fd, void *data) on fd activity
 * @param accept_handler Calls this function with args (int fd, void *data) on new connection
 *                       NULL for no callback or N/A
 * @param data Data to be passed through to handler functions
 * @return 0 on success, -1 on error
 */
int epoll_loop(int socket_fd, int epoll_fd, int batch_size, int timeout,
       bool oneshot,
       int (*connection_handler)(int, void*),
       int (*accept_handler)(int, void*),
       void *data);

/**
 * Initializes a new instance of an epoll file descriptor and adds a socket to it, listening
 * for input on that socket.
 * @param socket_fd Socket on which to listen for new connections.
 *                  -1 if creating epoll without socket.
 * @return epoll file descriptor
 */
int init_epoll(int socket_fd);

#endif
