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

struct blocking_socket_handler_init_payload {
    int port;
    int domain; //AF_INET
    int type; //SOCK_STREAM
    int protocol; //0 most of the time. refer to `man socket`
    unsigned long bind_ip; //fill with inet_addr, inet_pton(x.y.z.y) or give IN_ADDRANY
    int target_msu_type;
    int blocking;
};

struct blocking_socket_handler_data_payload {
    int fd;
};

#endif /* SOCKET_HANDLER_MSU_H_ */
