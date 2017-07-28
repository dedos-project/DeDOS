/**
 * @file socket_handler_msu.h
 * Listen/accept connection on a socket, then forward data to a given MSU
 */
#ifndef BLOCKING_SOCKET_HANDLER_MSU_H_
#define BLOCKING_SOCKET_HANDLER_MSU_H_

#include "generic_msu.h"
#include <sys/epoll.h>


/** All socket handler MSUs contain a reference to this type */
extern struct msu_type BLOCKING_SOCKET_HANDLER_MSU_TYPE;

/**
 * Adds the file descriptor to the epoll instance, without necessarily having the reference
 * to the socket handler MSU itself.
 * To be used from external MSUs.
 * @param fd The file descriptor to be added to epoll
 * @param events The events to monitor for (EPOLLIN, EPOLLOUT)
 * @return 0 on success, -1 on error
 */
int monitor_fd(int fd, uint32_t events);

#endif /* BLOCKING_SOCKET_HANDLER_MSU_H_ */
