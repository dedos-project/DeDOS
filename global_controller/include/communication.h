#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include "logging.h"
#include "control_protocol.h"
#include <stdint.h>
#include <pthread.h>

//for parsing msu id and thread info received from runtime
struct msu_thread_info {
    int msu_id;
    pthread_t thread_id;
};

int start_communication(int tcp_control_listen_port, char *output_filename);
void check_comm_sockets(void);
//void controller_rcv(struct runtime_endpoint *runtime_peer);
int send_to_runtime(int runtime_sock, char *buf, unsigned int bufsize);
int send_control_msg(int runtime_sock, struct dedos_control_msg *control_msg);

#endif
