#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <unistd.h>
#include <stdbool.h>

struct dedos_tcp_msg {
    int src_id;
    size_t data_len;
};

int send_to_endpoint(int fd, void *data, size_t data_len);
int init_bound_socket(int port);

#endif
