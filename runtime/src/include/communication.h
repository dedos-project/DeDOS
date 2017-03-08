#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include <sys/socket.h>
#include <linux/types.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/mman.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <stdint.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include "generic_msu.h"
#include "comm_protocol.h"
#include "control_msg_handler.h"

// #define comm_dbg(fmt, ...) \
//           do { if (1) { fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
//                                               __LINE__, __func__, __VA_ARGS__); } \
//                         fprintf(stderr,"\n"); } while (0)


#define MAX_RUNTIME_PEERS 12
#define MAX_RCV_BUFLEN 10000

struct runtime_endpoint {
     int sock;
     unsigned long ip;
};

// runtimes ipaddress
uint32_t runtimeIpAddress;
int runtime_listener_port; //port which all runtimes listen on
int controller_sock;
int tcp_comm_socket;
int ws_sock;
char *db_ip;
int db_port;
int db_max_load;

struct pending_runtimes{
    uint32_t *ips;
    unsigned int count;
};

struct pending_runtimes *pending_runtime_peers;

// queue pointer for the webserver MSU
struct generic_msu_queue *queue_ws;
struct generic_msu *msu_ws;
// queue for ssl read/write
struct generic_msu_queue *queue_ssl;
struct generic_msu *msu_ssl;

int dedos_control_socket_init(int tcp_port);
int dedos_webserver_socket_init(int listen_port);
//void dedos_control_send(struct dedos_intermsu_message* msg, struct msu_endpoint *endpoint, char *buf, unsigned int bufsize);
void dedos_control_send(struct dedos_intermsu_message* msg, int dst_runtime_ip, char *buf, unsigned int bufsize);
void dedos_control_rcv(int peer_sk);
int is_connected_to_runtime(uint32_t *test_ip);
int check_comm_sockets(void);
void print_dedos_intermsu_message(struct dedos_intermsu_message* msg);
int connect_to_runtime(char *ip, int tcp_port);
int connect_to_master(char *ip, int master_tcp_port);
void dedos_send_to_master(char* buf, unsigned int buflen);
void init_peer_socks(void);

#endif
