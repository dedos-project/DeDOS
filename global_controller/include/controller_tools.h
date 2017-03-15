#ifndef CONTROLLER_TOOLS_H_
#define CONTROLLER_TOOLS_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <linux/types.h>
#include <arpa/inet.h>
#include <math.h>
#include <stdlib.h>

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
    tcp_addr.sin_port = (unsigned short) htons(tcp_port);
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

static int how_many_digits(int num) {
    if (num >= 0) {
        return (int) floor(log10(abs((double) num)) + 1);
    } else {
        return -1;
    }
}

static inline uint32_t long_from(void *_p) {
    unsigned char *p = (unsigned char *)_p;
    uint32_t r, _p0, _p1, _p2, _p3;
    _p0 = p[0];
    _p1 = p[1];
    _p2 = p[2];
    _p3 = p[3];
    r = (_p3 << 24) + (_p2 << 16) + (_p1 << 8) + _p0;
    return r;
}

static inline int is_digit(char c) {
    if (c < '0' || c > '9') {
        return 0;
    }

    return 1;
}

static int string_to_ipv4(const char *ipstr, uint32_t *ip) {
    unsigned char buf[4] = { 0 };
    int cnt = 0;
    char p;

    if (!ipstr || !ip) {
        return -1;
    }

    while ((p = *ipstr++) != 0 && cnt < 4) {
        if (is_digit(p)) {
            buf[cnt] = (uint8_t) ((10 * buf[cnt]) + (p - '0'));
        } else if (p == '.') {
            cnt++;
        } else {
            return -1;
        }
    }
    /* Handle short notation */
    if (cnt == 1) {
        buf[3] = buf[1];
        buf[1] = 0;
        buf[2] = 0;
    }
    else if (cnt == 2) {
        buf[3] = buf[2];
        buf[2] = 0;
    }
    else if (cnt != 3) {
        /* String could not be parsed, return error */
        return -1;
    }

    *ip = long_from(buf);

    return 0;
}

static int ipv4_to_string(char *ipbuf, const uint32_t ip) {
    const unsigned char *addr = (const unsigned char *) &ip;
    int i;

    if (!ipbuf) {
        return -1;
    }

    for (i = 0; i < 4; i++) {
        if (addr[i] > 99) {
            *ipbuf++ = (char) ('0' + (addr[i] / 100));
            *ipbuf++ = (char) ('0' + ((addr[i] % 100) / 10));
            *ipbuf++ = (char) ('0' + ((addr[i] % 100) % 10));
        }
        else if (addr[i] > 9) {
            *ipbuf++ = (char) ('0' + (addr[i] / 10));
            *ipbuf++ = (char) ('0' + (addr[i] % 10));
        }
        else {
            *ipbuf++ = (char) ('0' + addr[i]);
        }

        if (i < 3)
            *ipbuf++ = '.';
    }
    *ipbuf = '\0';

    return 0;
}

#endif
