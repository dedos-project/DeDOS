#ifndef SOCKET_HANDLER_MSU_H_
#define SOCKET_HANDLER_MSU_H_
#include "msu_type.h"
#include "msu_message.h"

struct socket_msg {
    int fd;
};

int msu_monitor_fd(int fd, uint32_t events, struct local_msu *destination,
                   struct msu_msg_hdr *hdr);
int msu_remove_fd_monitor(int fd);

#define SOCKET_MSU_TYPE_ID 10
struct msu_type SOCKET_MSU_TYPE;

#endif
