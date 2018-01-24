/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
/**
 * @file communication.c
 *
 * General-purpose socket communication functions used from
 * global controller, runtime, or MSUs
 */
#include "dedos.h"
#include "communication.h"
#include "logging.h"

#include <arpa/inet.h>

/**
 * The maximum number of times that a call to `read()`
 * can be attempted for a single buffer before giving up
 */
#define MAX_READ_ATTEMPTS 200

int read_payload(int fd, size_t size, void *buff) {
    ssize_t rtn = 0;
    int attempts = 0;
    do {
        log(LOG_READS, "Attempting to read payload of size %d", (int)size - (int)rtn);
        int new_rtn = recv(fd, buff + rtn, size - rtn, 0);
        if (new_rtn < 0 && errno != EAGAIN) {
            log_perror("Error reading from fd: %d", fd);
            return -1;
        }
        if (attempts++ > MAX_READ_ATTEMPTS) {
            log_error("Attempted to read %d times", MAX_READ_ATTEMPTS);
            return -1;
        }
        if (new_rtn > 0)
            rtn += new_rtn;
    } while ((errno == EAGAIN || rtn > 0) && (int)rtn < (int)size);
    if (rtn == 0) {
        log(LOG_CONNECTIONS, "fd %d has been closed by peer", fd);
        return 1;
    }
    if (rtn != size) {
        log_error("Could not read full runtime payload from socket %d. "
                  "Requested: %d, received: %d", fd, (int)size, (int)rtn);
        return -1;
    }
    return 0;
}

ssize_t send_to_endpoint(int fd, void *data, size_t data_len) {
    ssize_t size = 0;
    while (size < data_len) {
        ssize_t rtn = write(fd, data + size, data_len - size);
        if (rtn <= 0) {
            log_perror("Error sending buffer %p to endpoint with socket %d", data + size, fd);
            break;
        } else if (rtn < data_len) {
            log_warn("Full buffer not sent to endpoint!");
        }
        size += rtn;
    }
    return size;
}

int _init_connected_socket(struct sockaddr_in *addr) {

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_perror("Failed to create socket");
        return -1;
    }

    // ???: Why set REUSEPORT and REUSEADDR on a socket that's not binding?
    int val = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,
                   &val, sizeof(val)) < 0 ) {
        log_perror("Error setting SO_REUSEPORT");
    }
    val = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                   &val, sizeof(val)) < 0) {
        log_perror("Error setting SO_REUSEADDR");
    }
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
    int port = ntohs(addr->sin_port);
    log(LOG_CONNECTIONS, "Attepting to connect to socket at %s:%d", ip, port);
    if (connect(sock, (struct sockaddr*)addr, sizeof(*addr)) < 0) {
        log_perror("Failed to connect to socket at %s:%d", ip, port);
        close(sock);
        return -1;
    }

    log(LOG_CONNECTIONS, "Connected socket to %s:%d", ip, port);
    return sock;
}

int _init_bound_socket(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        log_perror("Failed to create socket");
        return -1;
    }
    int val = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val)) < 0) {
        log_perror("Error setting SO_REUSEPORT on socket");
        return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        log_perror("Error setting SO_REUSEADDR on socket");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        log_perror("Failed to bind to port %d", port);
        return -1;
    }
    return sock;
}

/**
 * The backlog size for listening sockets
 */
#define BACKLOG 1024

int _init_listening_socket(int port) {
    int sock = init_bound_socket(port);
    if (sock < 0) {
        log_error("Could not start listening socket due to failed bind");
        return -1;
    }

    int rtn = listen(sock, BACKLOG);
    if (rtn < 0) {
        log_perror("Error starting listening socket");
        return -1;
    }
    log(LOG_COMMUNICATION, "Started listening socket on fd %d", sock);
    return sock;
}
