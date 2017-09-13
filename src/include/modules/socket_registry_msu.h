#ifndef SOCKET_REGISTRY_MSU_H_
#define SOCKET_REGISTRY_MSU_H_
#include <inttypes.h>

struct socket_registry_data {
    int fd;
    int msu_id;
    uint32_t events;
    uint32_t ip_address;
};

struct socket_registry_data *init_socket_registry_data(
        int fd, int msu_id, uint32_t events, uint32_t ip_address);

const struct msu_type SOCKET_REGISTRY_MSU_TYPE;
#endif
