#ifndef SOCKET_HANDLER_MSU_H_
#define SOCKET_HANDLER_MSU_H_
#include "msu_type.h"

struct socket_msg {
    int fd;
};

#define SOCKET_MSU_TYPE_ID 10
struct msu_type SOCKET_MSU_TYPE;

#endif
