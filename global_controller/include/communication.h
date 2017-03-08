#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include "logging.h"
#include <stdint.h>
#include <pthread.h>

#define MAX_RUNTIMES 20
#define MAX_RCV_BUFLEN 4096

/**
 * The runtime properties represent what is made available
 * to the runtime process on the physical node
 */

struct runtime_endpoint {
    int id;
    int sock;
    uint32_t ip;
    uint16_t port;
    uint16_t num_cores;
    uint64_t dram; // 0 to 1.844674407×10^19
    uint64_t io_network_bw; // 0 to 1.844674407×10^19
    uint32_t num_pinned_threads;
    uint32_t num_threads;
};

//for parsing msu id and thread info received from runtime
struct msu_thread_info {
    int msu_id;
    pthread_t thread_id;
};

int start_communication(int tcp_control_listen_port);
void check_comm_sockets(void);
//void controller_rcv(struct runtime_endpoint *runtime_peer);
int send_to_runtime(int runtime_sock, char *buf, unsigned int bufsize);

#endif
