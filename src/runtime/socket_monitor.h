#ifndef SOCKET_MONITOR_H_
#define SOCKET_MONITOR_H_

int monitor_controller_socket(int fd);
int monitor_runtime_socket(int fd);

int run_socket_monitor(int local_port, struct sockaddr_in *ctrl_addr);

#endif
