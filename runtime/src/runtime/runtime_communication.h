#ifndef RUNTIME_COMMUNICATION_H_
#define RUNTIME_COMMUNICATION_H_
#include <unistd.h>
#include <stdbool.h>
#include <netinet/ip.h>

int connect_to_runtime_peer(unsigned int id, struct sockaddr_in *addr);
int send_to_peer(unsigned int runtime_id, size_t data_size, void *data);
int add_runtime_peer(unsigned int runtime_id, int fd);
int init_runtime_socket(int listen_port);
int handle_runtime_communication(int fd);

#endif
