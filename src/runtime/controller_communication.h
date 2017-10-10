#ifndef CONTROLLER_COMMUNICATION_H_
#define CONTROLLER_COMMUNICATION_H_
#include "netinet/ip.h"
#include "ctrl_runtime_messages.h"
#include "rt_controller_messages.h"
#include <stdbool.h>

int send_to_controller(struct rt_controller_msg_hdr *msg, void *payload);
int init_controller_socket(struct sockaddr_in *addr);
int handle_controller_communication(int fd);
bool is_controller_fd(int fd);
int send_stats_to_controller();

#endif
