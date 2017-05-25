#include <stdio.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/select.h>
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

#include "api.h"
#include "communication.h"
#include "logging.h"
#include "control_msg_handler.h"
#include "stat_msg_handler.h"
#include "controller_tools.h"
#include "dfg.h"

static fd_set readfds;
int control_listen_sock;
int max_fd;

int stat_listen_sock;

static void cleanup_peer_socket(struct dfg_runtime_endpoint *runtime_peer){
    log_info("Removing socket %d", runtime_peer->sock);
    close(runtime_peer->sock);
    runtime_peer->sock = 0;
    show_connected_peers();
}

static void controller_rcv(struct dfg_runtime_endpoint *runtime_peer){
    char rcv_buf[MAX_RCV_BUFLEN];
    struct sockaddr_in remote_addr;

    memset((char *) &rcv_buf, 0, sizeof(rcv_buf));
    socklen_t len = sizeof(remote_addr);
    ssize_t data_len = 0;
    data_len = recvfrom(runtime_peer->sock, rcv_buf, MAX_RCV_BUFLEN, 0,
            (void*) &remote_addr, &len);
    //debug("DEBUG: Received msg from peer of len: %u",data_len);
    if (data_len == -1) {
        debug("ERROR: %s","Couldn't receive any data from runtime");
    } else if (data_len == 0) {
        FD_CLR(runtime_peer->sock, &readfds);
        cleanup_peer_socket(runtime_peer);
        return;
    } else {
        process_runtime_msg(rcv_buf, runtime_peer->sock);
    }
}

void check_comm_sockets(void) {
    int ret;
    struct timeval timeout;
    int opt = 1;
    socklen_t len;
    struct sockaddr_storage addr;
    int i;
    struct dfg_config *dfg = NULL;

    dfg = get_dfg();
    len = sizeof(addr);

    FD_ZERO(&readfds);
    FD_SET(control_listen_sock, &readfds);

    max_fd = control_listen_sock;

    pthread_mutex_lock(&dfg->dfg_mutex);

    for (i = 0; i < dfg->runtimes_cnt; i++) {
        if (dfg->runtimes[i]->sock > 0) {
            FD_SET(dfg->runtimes[i]->sock, &readfds);

            if (max_fd < dfg->runtimes[i]->sock) {
                max_fd = dfg->runtimes[i]->sock;
            }
        }
    }

    pthread_mutex_unlock(&dfg->dfg_mutex);

    timeout.tv_sec = 0; // in second
    timeout.tv_usec = 1000;
    ret = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
    // printf("After select call\n");

    if ((ret < 0) && (errno != EINTR)) {
        debug("ERROR: %s", "select() failed");
    }
    if (ret == 0) {
        return;
    }

    if (FD_ISSET(control_listen_sock, &readfds)) {
        //a new connection from some other runtime peer
        int peer_sk;
        peer_sk = accept(control_listen_sock, (struct sockaddr*)&addr, &len);
        debug("DEBUG: peer_sk after accept on control socket: %d", peer_sk);
        if (peer_sk < 0) {
            debug("ERROR: %s",
                    "Can't accept new connection on tcp control socket");
            return;
        }
        if (setsockopt(peer_sk, IPPROTO_TCP, TCP_NODELAY, (void *) &opt,
                sizeof(opt))) {
            debug("ERROR: %s", "setting TCP_NODELAY");
        }
        if (addr.ss_family != AF_INET) {
            debug("ERROR: Not AF_INET family: %d\n", addr.ss_family);
            close(peer_sk);
            return;
        }

        struct sockaddr_in *s = (struct sockaddr_in *) &addr;
        char peer_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &s->sin_addr, peer_ip, sizeof(peer_ip));

        add_runtime(peer_ip, peer_sk);

    } else {
        // listen to already registerd runtimes

        for (i = 0; i < dfg->runtimes_cnt; i++) {
            pthread_mutex_lock(&dfg->dfg_mutex);

            if (FD_ISSET(dfg->runtimes[i]->sock, &readfds)) {
                pthread_mutex_unlock(&dfg->dfg_mutex);

                //call message receiver function
                controller_rcv(dfg->runtimes[i]);
            }

            pthread_mutex_unlock(&dfg->dfg_mutex);
        }
    }
}

int send_control_msg(int runtime_sock, struct dedos_control_msg *control_msg){
    int total_msg_size = control_msg->header_len + control_msg->payload_len;

    char buf[total_msg_size];

    memcpy(buf, control_msg, control_msg->header_len);
    memcpy(buf + control_msg->header_len, control_msg->payload, control_msg->payload_len);

    return send_to_runtime(runtime_sock, buf, (unsigned int)total_msg_size);
}

int send_to_runtime(int runtime_sock, char *buf, unsigned int bufsize){
    log_debug("Sending msg to peer runtime with len: %u", bufsize);
    int data_len;
    data_len = (int)send(runtime_sock, buf, bufsize, 0);
    if (data_len == -1) {
        log_error("failed to send data on socket");
        return -1;
    }
    if (data_len != bufsize) {
        log_warn("Failed to send full message to runtime socket %d", runtime_sock);
        return -1;
    }
    log_info("Sent msg to peer runtime with len: %d", data_len);
    return 0;
}

int start_communication(int tcp_control_listen_port)
{
    int ret;

    control_listen_sock = start_listener_socket(tcp_control_listen_port, control_listen_sock);

    if (!control_listen_sock > 0) {
        ret = -1;
    }
    else {
        ret = 0;
    }

    while (1) {
        check_comm_sockets();
    }
    return ret;
}
