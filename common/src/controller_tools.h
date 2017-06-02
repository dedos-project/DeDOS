#ifndef CONTROLLER_TOOLS_H_
#define CONTROLLER_TOOLS_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <linux/types.h>
#include <arpa/inet.h>
#include <math.h>
#include <stdlib.h>

#include "ip_utils.h"
#include "logging.h"

static int start_listener_socket(int tcp_port, int sock) {
    //start listen socket
    debug("Init tcp socket on port: %d", tcp_port);

    struct sockaddr_in tcp_addr;
    socklen_t slen = sizeof(tcp_addr);

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        debug("ERROR: %s", "Failed to create TCP socket");
        return -1;
    }
    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval,
            sizeof(optval)) < 0) {
        debug("ERROR: %s", "Failed to set SO_REUSEPORT");
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval,
            sizeof(optval)) < 0) {
        debug("ERROR: %s", "Failed to set SO_REUSEADDR");
    }

    memset((char *) &tcp_addr, 0, slen);
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons((uint16_t) tcp_port);
    tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *) &tcp_addr, slen) == -1) {
        debug("ERROR: %s", "Failed to bind to TCP socket");
        return -1;
    }

    if (listen(sock, 10) < 0) {
        debug("ERROR: %s", "Failed to listen on tcp socket");
        return -1;
    }
    debug("INFO: Started listening on TCP socket %d on fd %d",
            sock, tcp_port);

    return sock;
}

#endif
