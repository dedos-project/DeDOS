#ifndef CONTROLLER_COMMUNICATION_H_
#define CONTROLLER_COMMUNICATION_H_
#include "netinet/ip.h"
#include "ctrl_runtime_messages.h"

int send_to_controller(struct control_msg *msg, void *data);
int init_controller_socket(struct sockaddr_in *addr);
int handle_controller_communication(int fd);
// TODO: is_controller_fd
int is_controller_fd(int fd);

#endif
