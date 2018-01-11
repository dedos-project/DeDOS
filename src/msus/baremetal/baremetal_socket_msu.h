#ifndef BAREMETAL_SOCKET_MSU_H__
#define BAREMETAL_SOCKET_MSU_H__

#define BAREMETAL_SOCK_MSU_TYPE_ID 110

struct baremetal_msg {
    int n_hops;
    int fd;
};

struct msu_type BAREMETAL_SOCK_MSU_TYPE;

#endif
