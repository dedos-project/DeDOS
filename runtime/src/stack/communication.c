#include <netinet/tcp.h>
#include <error.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include "stats.h"
#include "communication.h"
#include "runtime.h"
#include "comm_protocol.h"
#include "routing.h"
#include "modules/msu_tcp_handshake.h"
#include "control_msg_handler.h"
#include "generic_msu_queue.h"
#include "generic_msu_queue_item.h"
#include "msu_tracker.h"
#include "global.h"
#include "modules/ssl_msu.h"
#include "logging.h"
#include "modules/ssl_request_routing_msu.h"
#include "modules/baremetal_msu.h"
#include <assert.h>
static fd_set readfds;
struct sockaddr_in ws, cli_addr, bm;
int clilen = sizeof(cli_addr);

//FIXME (Non-critical) Replace tracking of connected runtimes with a struct runtime_endpoint

//will contain established socket descriptor for each connected work
int peer_tcp_sockets[MAX_RUNTIME_PEERS];
//list of IP address, entry at same index i will correspond to the socket for a particular IP
uint32_t peer_ips[MAX_RUNTIME_PEERS];

int is_connected_to_runtime(uint32_t *test_ip){
    int i;
    for (i = 0; i < MAX_RUNTIME_PEERS; ++i) {
        if(*test_ip != 0 && peer_ips[i] == *test_ip){
            return 1;
        }
    }
    return 0;
}

static int get_peer_socket(uint32_t ip_addr)
{
    int i, worker_sock = 0;

    for (i = 0; i < MAX_RUNTIME_PEERS; i++) {
        if (peer_ips[i] == ip_addr) {
            worker_sock = peer_tcp_sockets[i];
            if (worker_sock == 0) {
                log_error("%s","Missing worker socket for corresponding IP");
                break;
            }
            return worker_sock;
        }
    }
    return -1;
}

static void cleanup_peer_socket(int peer_sk)
{
    int i;
    for (i = 0; i < MAX_RUNTIME_PEERS; i++) {
        if (peer_tcp_sockets[i] == peer_sk) {
            peer_ips[i] = 0;
            peer_tcp_sockets[i] = 0;
            close(peer_sk);
        }
    }
    log_info("Removed socket %d", peer_sk);
    char ipstr[INET_ADDRSTRLEN];
    log_info("INFO: %s", "Currently open peer socket ->");
    for (i = 0; i < MAX_RUNTIME_PEERS; ++i) {
        ipv4_to_string(ipstr, peer_ips[i]);
        log_info("Socket: %d, IP: %s", peer_tcp_sockets[i], ipstr);
    }
}

int dedos_control_socket_init(int tcp_control_port)
{
    //start listen socket
    int max_fd;
    struct sockaddr_in tcp_addr;
    socklen_t slen = sizeof(tcp_addr);
    int optval = 1;

    FD_ZERO(&readfds);

    if ((tcp_comm_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        log_error("%s","Failed to create TCP socket");
        return -1;
    }
    if (setsockopt(tcp_comm_socket, SOL_SOCKET, SO_REUSEPORT, &optval,
            sizeof(optval)) < 0) {
        log_error("%s","Failed to set SO_REUSEPORT");
    }
    if (setsockopt(tcp_comm_socket, SOL_SOCKET, SO_REUSEADDR, &optval,
            sizeof(optval)) < 0) {
        log_error("%s","Failed to set SO_REUSEADDR");
    }

    // to get my own ip

    memset((char *) &tcp_addr, 0, slen);
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(tcp_control_port);
    //TODO Maybe only on an internal interface and not the external one?
    tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    struct ifreq ifr;
    char array[] = "em1";

    //Type of address to retrieve - IPv4 IP address
    ifr.ifr_addr.sa_family = AF_INET;
    //Copy the interface name in the ifreq structure
    strncpy(ifr.ifr_name , array , IFNAMSIZ - 1);
    ioctl(tcp_comm_socket, SIOCGIFADDR, &ifr);
    runtimeIpAddress = (((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr).s_addr;
    if (bind(tcp_comm_socket, (struct sockaddr *) &tcp_addr, slen) == -1) {
        log_error("%s","Failed to bind to TCP socket");
        return -1;
    }

    if (listen(tcp_comm_socket, 10) < 0) {
        log_error("%s","Failed to listen of tcp control socket");
        return -1;
    }
    log_info("Started listening on TCP control socket on port %d",
            tcp_control_port);
    max_fd = tcp_comm_socket;
    // FD_SET(tcp_comm_socket, &readfds);

    return 0;
}

int dedos_webserver_socket_init(int webserver_port) {
    // Setup the webserver socket
    ws_sock = socket(AF_INET, SOCK_STREAM, 0);
    int Set = 1;
    if ( setsockopt(ws_sock, SOL_SOCKET, SO_REUSEADDR, &Set, sizeof(int)) == -1 ) {
        log_warn("setsockopt() failed with error %s (%d)", strerror(errno), errno );
    }

    ws.sin_family = AF_INET;
    ws.sin_addr.s_addr = INADDR_ANY;
    ws.sin_port = htons(webserver_port);

    int optval = 1;
    if (setsockopt(ws_sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        log_error("%s","Failed to set SO_REUSEPORT");
    }

    if ( bind(ws_sock, (struct sockaddr*) &ws, sizeof(ws)) == -1 ) {
        printf("bind() failed\n");
        return -1;
    }

    listen(ws_sock, 5);

    log_info("Started webserver listen port: %d",webserver_port);

    return 0;
}


int dedos_baremetal_listen_socket_init(int baremetal_listen_port){
    // Setup the baremetal msu listen socket
    baremetal_sock = socket(AF_INET, SOCK_STREAM, 0);
    int Set = 1;
    if ( setsockopt(baremetal_sock, SOL_SOCKET, SO_REUSEADDR, &Set, sizeof(int)) == -1 ) {
        log_warn("setsockopt() failed with error %s (%d)", strerror(errno), errno );
    }

    bm.sin_family = AF_INET;
    bm.sin_addr.s_addr = INADDR_ANY;
    bm.sin_port = htons(baremetal_listen_port);

    int optval = 1;
    if (setsockopt(baremetal_sock, SOL_SOCKET, SO_REUSEPORT, &optval,
            sizeof(optval)) < 0) {
        log_error("%s","Failed to set SO_REUSEPORT");
    }

    if ( bind(baremetal_sock, (struct sockaddr*) &bm, sizeof(bm)) == -1 ) {
        printf("bind() failed\n");
        return -1;
    }

    listen(baremetal_sock, 5);

    log_info("Started baremetal listen port: %d",baremetal_listen_port);

    return 0;
}


int check_comm_sockets() {
    int opt = 1;
    socklen_t len;
    struct sockaddr_storage addr;
    int i, j;
    int max_fd;
    len = sizeof(addr);
    // implementing poll
#ifdef DEDOS_SUPPORT_BAREMETAL_MSU
    int totalSockets = 4;
#else
    int totalSockets = 3;
#endif

    struct pollfd fds[totalSockets + MAX_RUNTIME_PEERS];
    fds[0].fd = tcp_comm_socket;
    fds[0].events = POLLIN;
    fds[1].fd = controller_sock;
    fds[1].events = POLLIN;
    fds[2].fd = ws_sock;
    fds[2].events = POLLIN;
#ifdef DEDOS_SUPPORT_BAREMETAL_MSU
    fds[3].fd = baremetal_sock;
    fds[3].events = POLLIN;
#endif

    for (j = 0; j < MAX_RUNTIME_PEERS; j++) {
        if(peer_tcp_sockets[j] > 0) {
            fds[totalSockets + j].fd = peer_tcp_sockets[j];
            fds[totalSockets + j].events = POLLIN;
        }
    }

    int ret = poll(&fds, totalSockets + MAX_RUNTIME_PEERS, 0);

    if (ret != 0) {
        if (ret == -1) {
            printf("Error in Poll\n");
        } else if (fds[0].revents & POLLIN) {
            fds[0].revents = 0;
           int peer_sk;
            peer_sk = accept(tcp_comm_socket, NULL, NULL);
            if (peer_sk < 0) {
                log_error("%s","Can't accept new connection on tcp control socket");
            }
            if (setsockopt(peer_sk, IPPROTO_TCP, TCP_NODELAY, (void *) &opt,
                    sizeof(opt))) {
                log_error("%s","Error setting TCP_NODELAY");
            }
            if (getpeername(peer_sk, (struct sockaddr*) &addr, &len)) {
                log_error("%s","Failed to get peer info");
            }
            if (addr.ss_family != AF_INET) {
                log_error("Not AF_INET family: %d", addr.ss_family);
                close(peer_sk);
                return 0;
            }
            //need to add it to books to keep track of this socket
            int i;
            for (i = 0; i < MAX_RUNTIME_PEERS; i++) {
                if (peer_tcp_sockets[i] == 0 && peer_ips[i] == 0) {

                    struct sockaddr_in *s = (struct sockaddr_in *) &addr;

                    peer_tcp_sockets[i] = peer_sk;
                    peer_ips[i] = s->sin_addr.s_addr;

                    char ipstr[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
                    log_debug("Stored socket %d info for peer at: %s\n", peer_sk,
                            ipstr);
                    break;
                }
            }
            if (i == MAX_RUNTIME_PEERS) {
                log_error("%s","FATAL!: Cannot find any empty slots to store new connection");
            }
        } else if(fds[1].revents & POLLIN) {
            fds[1].revents = 0;
            //control info from global controller
            char rcv_buf[MAX_RCV_BUFLEN];

            memset((char *) &rcv_buf, 0, sizeof(rcv_buf));
            struct sockaddr_in remote_addr;
            socklen_t len = sizeof(remote_addr);
            ssize_t data_len = 0;
            data_len = recvfrom(controller_sock, rcv_buf, MAX_RCV_BUFLEN, 0,
                    (void*) &remote_addr, &len);
            //TODO More check on incoming message sanity for now,
            //assuming control communication is safe

            if (data_len == -1) {
                log_error("%s", "while receiving TCP data over controller_sock");
            } else {
                log_debug("Received data with len: %d", data_len);
                if (data_len == 0) {
                    log_error("%s", "FATAL: Global controller_sock failure");
                    return -1;;
                }
                //parse received cmd
                parse_cmd_action(&rcv_buf);
            }
        } else if (fds[2].revents & POLLIN) {
            fds[2].revents = 0;
            // received something on the websocket
            // accept this and then pass it to the queue acceessed by the global variable for now
            // if the queue is NULL close the connection right away
            // printf("Trying to get a new conn \n");
            struct sockaddr_in ClientAddr;
            socklen_t AddrLen = sizeof(ClientAddr);
            int new_sock_ws = accept(ws_sock,(struct sockaddr*) &ClientAddr, &AddrLen);

            if (new_sock_ws < 0) {
                log_warn("accept(%d,...) failed with error %s ", ws_sock, strerror(errno));
                return -1;
            }

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 10000;
            if (setsockopt(new_sock_ws, SOL_SOCKET, SO_RCVTIMEO,
                            (char *) &timeout, sizeof(struct timeval)) == -1) {
                log_warn("setsockopt(%d,...) failed with error %s ", new_sock_ws, strerror(errno));
                return -1;
            }

            // printf("Accepted a new conn : %d \n", new_sock_ws);
            // if (queue_ssl == NULL || queue_ws == NULL) {
            //     log_error("%s","************** WS: Not all msu's wanted initialized ********************");
            //     close(new_sock_ws);
            if (ssl_request_routing_msu == NULL) {
                log_error("*ssl_request_routing_msu is NULL, forgot to create it?%s","");
                close(new_sock_ws);
            } else {
                // enqueue this item into this queue so that the MSU can process it
                struct ssl_data_payload *data = (struct ssl_data_payload*) malloc(sizeof(struct ssl_data_payload));
                memset(data, '\0', MAX_REQUEST_LEN);
                data->socketfd = new_sock_ws;
                // set the initial type to READ
                data->type = READ;
                data->state = NULL;
                // get a new queue item and enqueue it into this queue
                struct generic_msu_queue_item *new_item_ws = malloc(sizeof(struct generic_msu_queue_item));
                new_item_ws->buffer = data;
                new_item_ws->buffer_len = sizeof(struct ssl_data_payload);
#ifdef DATAPLANE_PROFILING
                new_item_ws->dp_profile_info.dp_id = get_request_id();
#endif
    //            generic_msu_queue_enqueue(queue_ssl, new_item_ws);
                int ssl_request_queue_len = generic_msu_queue_enqueue(&ssl_request_routing_msu->q_in, new_item_ws); //enqueuing to routing MSU
                if (ssl_request_queue_len < 0) {
                    log_error("Failed to enqueue ssl_request to %s",ssl_request_routing_msu->type->name);
                    free(new_item_ws);
                    free(data);
                } else {
                    log_debug("Enqueued ssl forwarding request, q_len: %u",ssl_request_queue_len);
                }
            }
#ifdef DEDOS_SUPPORT_BAREMETAL_MSU
        } else if (fds[3].revents & POLLIN) {
            //BAREMETAL MSU data entry point into dedos
            fds[3].revents = 0;
            // accept this and then pass it to the queue acceessed by the global variable for now
            // if the queue is NULL close the connection right away
            struct sockaddr_in ClientAddr;
            socklen_t AddrLen = sizeof(ClientAddr);
            int new_sock_bm = accept(baremetal_sock,(struct sockaddr*) &ClientAddr, &AddrLen);

            if (new_sock_bm < 0){
                log_warn("accept(%d,...) failed with error %s ", baremetal_sock, strerror(errno));
                return -1;
            }
            log_debug("Accepted conn for baremetal");
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 10000;
            if ( setsockopt(new_sock_bm, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(struct timeval)) == -1 ) {
                log_warn("setsockopt(%d,...) failed with error %s ", new_sock_bm, strerror(errno));
                return -1;
            }
            if (baremetal_entry_msu == NULL) {
                log_error("baremetal entry msu is NULL, forgot to create it?%s","");
                close(new_sock_bm);
            } else {
                // enqueue this item into this queue so that the MSU can process it
                struct baremetal_msu_data_payload *data = (struct baremetal_msu_data_payload*)
                                                        malloc(sizeof(struct baremetal_msu_data_payload));
                data->socketfd = new_sock_bm;
                // set the initial type to READ
                data->type = NEW_ACCEPTED_CONN;
                data->int_data = 0;
                // get a new queue item and enqueue it into this queue
                struct generic_msu_queue_item *new_item_bm = malloc(sizeof(struct generic_msu_queue_item));
                if(!new_item_bm){
                    log_error("Failed to mallc new bm item");
                    return -1;
                }
                memset(new_item_bm,'\0',sizeof(struct generic_msu_queue_item));
                new_item_bm->buffer = data;
                new_item_bm->buffer_len = sizeof(struct baremetal_msu_data_payload);
#ifdef DATAPLANE_PROFILING
                new_item_bm->dp_profile_info.dp_id = get_request_id();
#endif
                int baremetal_entry_msu_q_len= generic_msu_queue_enqueue(&baremetal_entry_msu->q_in, new_item_bm); //enqueuing to first bm MSU
                if(baremetal_entry_msu_q_len < 0){
                    log_error("Failed to enqueue baremetal request to entry msu with id: %d",baremetal_entry_msu->id);
                    free(new_item_bm);
                    free(data);
                } else {
                    log_debug("Enqueued baremtal request in entry msu, q_len: %d",baremetal_entry_msu_q_len);
                }
            }
#endif
       } else {
            // check for the other runtime sockets
            for (j = 0; j < MAX_RUNTIME_PEERS; j++) {
                if(fds[j + totalSockets].revents & POLLIN && peer_tcp_sockets[j] > 0) {
                    fds[j + totalSockets].revents = 0;
                    if(peer_tcp_sockets[j] > 0){
                        dedos_control_rcv(peer_tcp_sockets[j]);
                    }
                }
            }
        }
    }
    return 0;
}

int connect_to_runtime(char *ip, int tcp_control_port)
{
    int sockfd, portno, n;
    struct sockaddr_in peer_addr;

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_error("%s"," failure opening socket");
        return -1;
    }
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))
            < 0) {
        log_error("%s","Failed to set SO_REUSEPORT");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))
            < 0) {
        log_error("%s","Failed to set SO_REUSEADDR");
    }
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *) &optval, sizeof(optval))) {
        log_error("%s","Error setting TCP_NODELAY");
    }

    /* build the server's Internet address */
    bzero((char *) &peer_addr, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(peer_addr.sin_addr));
    peer_addr.sin_port = htons(tcp_control_port);

    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr_in*) &peer_addr, sizeof(peer_addr))
            < 0) {
        log_error("Connection to runtime failed: %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    log_info("Connected to remote peer %s", ip);

    /* book keep connected socket */
    int i;
    for (i = 0; i < MAX_RUNTIME_PEERS; i++) {
        if (peer_tcp_sockets[i] == 0 && peer_ips[i] == 0) {

            struct sockaddr_in *s = (struct sockaddr_in *) &peer_addr;

            peer_tcp_sockets[i] = sockfd;
            peer_ips[i] = s->sin_addr.s_addr;

            char ipstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
            log_debug("Stored socket %d info for worker: %s", sockfd, ipstr);
            break;
        }
    }
    return 0;
}

void init_peer_socks(void){
    int i = 0;
    for (i = 0; i < MAX_RUNTIME_PEERS; i++) {
        peer_tcp_sockets[i] = 0;
        peer_ips[i] = 0;
    }
}

int connect_to_master(char *ip, int master_port)
{
    int portno, n;
    struct sockaddr_in peer_addr;

    /* socket: create the socket */
    controller_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (controller_sock < 0) {
        log_error("%s","Failed to create socket");
        return -1;
    }
    int optval = 1;
    if (setsockopt(controller_sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))
            < 0) {
        log_error("%s","Failed to set SO_REUSEPORT");
    }
    if (setsockopt(controller_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))
            < 0) {
        log_error("%s","Failed to set SO_REUSEADDR");
    }

    /* build the server's Internet address */
    bzero((char *) &peer_addr, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(peer_addr.sin_addr));
    peer_addr.sin_port = htons(master_port);

    /* connect: create a connection with the server */
    if (connect(controller_sock, (struct sockaddr_in*) &peer_addr, sizeof(peer_addr))
            < 0) {
        log_error("Failed to connect to master: %s", strerror(errno));
        close(controller_sock);
        return -1;
    }
    log_info("Connected to global controller at: %s", ip);
    return 0;
}

void dedos_control_send(struct dedos_intermsu_message* msg,
        int dst_runtime_ip, char *buf, unsigned int bufsize)
//        struct msu_endpoint *endpoint, char *buf, unsigned int bufsize)
{
    //get ip from endpoint
    //find the socket for ip in the list of peer sockets.
    int peer_sk;
    uint32_t ip_addr = dst_runtime_ip;
    peer_sk = get_peer_socket(ip_addr);
    if (peer_sk < 0) {
        log_error("%s", "No TCP channel with destination runtime");
        return;
    }

    //create output buffer
    size_t sendbuf_len = sizeof(struct dedos_intermsu_message) + bufsize;


    char *sendbuf = (char*) malloc(sizeof(size_t) + sendbuf_len);
    memcpy(sendbuf, &sendbuf_len, sizeof(size_t));
    memcpy(sendbuf + sizeof(size_t), msg, sizeof(struct dedos_intermsu_message));
    memcpy(sendbuf + sizeof(size_t) + sizeof(struct dedos_intermsu_message), buf, bufsize);
    log_debug("%s", "Sending following inter dedos message: ");
    print_dedos_intermsu_message(msg);

    log_debug("******* Sending data with len: %u", sendbuf_len);
    /*
     struct sockaddr_in dst_addr;
     socklen_t d_len = sizeof(dst_addr);
     dst_addr.sin_family = AF_INET;
     dst_addr.sin_port = htons(tcp_control_port);
     dst_addr.sin_addr.s_addr = endpoint->ipv4.addr;
     */
    int bytes_sent;
    if ((bytes_sent = send(peer_sk, sendbuf, sendbuf_len + sizeof(size_t), 0)) == -1) {
        log_error("%s", "sendto failed");
        goto err;
    }
    log_debug("Finished sending data over control port len: %u *********",
            bufsize);

    aggregate_stat(MESSAGES_SENT, 0, 1, 1);
    aggregate_stat(BYTES_SENT, 0, bytes_sent, 1);
    err: free(sendbuf);
}

void dedos_control_rcv(int peer_sk)
{
    int ret;
    char rcv_buf[MAX_RCV_BUFLEN];
    struct dedos_intermsu_message *msg;
    struct generic_msu *dst_msu;

    memset((char *) &rcv_buf, 0, sizeof(rcv_buf));
    struct sockaddr_in remote_addr;
    socklen_t len = sizeof(remote_addr);

    ssize_t data_len = 0;
    size_t msg_size = -1;
    data_len = recvfrom(peer_sk, &msg_size, sizeof(size_t), 0, (void*)&remote_addr, &len);
    if (data_len != sizeof(ssize_t)){
        log_error("Couldn't read the size of incoming message");
    }
    if (data_len == -1){
        log_error("%s", strerror(errno));
    }

    data_len = 0;
    while (data_len < msg_size){
        len = sizeof(remote_addr);
        int new_len = recvfrom(peer_sk, rcv_buf+data_len, msg_size - data_len, 0,
                (void*) &remote_addr, &len);
        if (new_len < 0){
            log_error("Error reading");
            return;
        } else {
            data_len += new_len;
            if (data_len < msg_size)
                log_debug("Reading more...");
        }
    }
    aggregate_stat(MESSAGES_RECEIVED, 0, 1, 1);
    aggregate_stat(BYTES_RECEIVED, 0, data_len, 1);
    //TODO More check on incoming message sanity for now,
    //assuming inter-runtime communication is safe

    if (data_len != msg_size){
        log_warn("Received data len of %d, expected %d", (int)data_len, (int)msg_size);
    }

    if (data_len == MAX_RCV_BUFLEN){
        log_warn("Received maximum sized data. May not have received full message");
    }

    if (data_len == -1) {
        log_error("Error receiving TCP data over control socket: %s", strerror(errno));
    } else {
        log_debug("************Received data with len: %d ***", data_len);
        if (data_len == 0) {
            FD_CLR(peer_sk, &readfds);
            cleanup_peer_socket(peer_sk);
            return;
        }
        //parse received buffer
        msg = (struct dedos_intermsu_message *) rcv_buf;
        msg->payload =
                (char*) (rcv_buf + sizeof(struct dedos_intermsu_message));
        log_debug("%s", "Received following inter dedos message");
        print_dedos_intermsu_message(msg);

        //Find destination msu pointer based on dst_msu_id to call restore of that
        //msu which should either enqueue or do whatever needs to be done with the data
        if (!msg->dst_msu_id) {
            log_warn("%s", "Invalid msg, missing dst_msu_id in message");
            return;
        }

		struct msu_placement_tracker *msu_tracker;
		msu_tracker = msu_tracker_find(msg->dst_msu_id);
		if(!msu_tracker){
				log_error("Couldn't find the msu_tracker for find the thread with MSU %d", msg->dst_msu_id);
				return;
		}

		int r = get_thread_index(msu_tracker->dedos_thread->tid);
		if(r == -1){
			log_error("Cannot find thread index for thread %u",msu_tracker->dedos_thread->tid);
			return;
		}
//        int r = msg->dst_msu_id % 8;
        struct dedos_thread *tmp = &all_threads[r];

        dst_msu = dedos_msu_pool_find(tmp->msu_pool, msg->dst_msu_id);
        if (!dst_msu) {
            log_error("Missing dst_msu_id: %d in runtime", msg->dst_msu_id);
            //TODO propagate error info to src
            return;
        }

        log_debug("Enqueuing request in queue of MSU ID %d", dst_msu->id);
        dst_msu->type->deserialize(dst_msu, msg, (void*) msg->payload, msg->payload_len);
    }
}

void print_dedos_intermsu_message(struct dedos_intermsu_message* msg)
{
    log_debug("%s", "InterMSU Message header->");
    log_debug("SRC MSU_ID: %d", msg->src_msu_id);
    log_debug("DST MSU_ID: %d", msg->dst_msu_id);
    log_debug("PROTO_MSG_TYPE: %u", msg->proto_msg_type);
    log_debug("PAYLOAD_LEN: %u", msg->payload_len);
}

void dedos_send_to_master(char* buf, unsigned int buflen)
{

    if (send(controller_sock, buf, buflen, 0) == -1) {
        log_error("%s", "sendto failed");
        return;
    }
//    log_debug("***Sent data to master with len: %u", buflen);
}
