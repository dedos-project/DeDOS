#ifndef DEDOS_H_
#define DEDOS_H_

#include <netinet/ip.h>

int dedos_close(int fd);
#define close dedos_close

int dedos_open(const char *path, int oflag);

int dedos_socket(int domain, int type, int protocol);
#define socket dedos_socket

int dedos_accept(int socket, struct sockaddr *restrict address, socklen_t *restrict address_len);
#define accept dedos_accept

#endif
