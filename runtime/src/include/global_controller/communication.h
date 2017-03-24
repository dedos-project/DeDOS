#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include "logging.h"
#include "msu_tracker.h" // For getting msu_thread_info which was redefined here
#include <stdint.h>
#include <pthread.h>

int start_communication(int tcp_control_listen_port);
void check_comm_sockets(void);
//void controller_rcv(struct runtime_endpoint *runtime_peer);
int send_to_runtime(int runtime_sock, char *buf, unsigned int bufsize);

#endif
