#include "communication.h"
#include "runtime_dfg.h"
#include "logging.h"

#include <arpa/inet.h>

int read_payload(int fd, size_t size, void *buff) {
    ssize_t rtn = read(fd, buff, size);
    if (rtn != size) {
        log_error("Could not read full runtime payload from socket %d", fd);
        return -1;
    }
    return 0;
}

ssize_t send_to_endpoint(int fd, void *data, size_t data_len) {
    ssize_t rtn = write(fd, data, data_len);
    if (rtn <= 0) {
        log_error("Error sending buffer to endpoint with socket %d", fd);
    } else if (rtn < data_len) {
        log_error("Full buffer not sent to endpoint!");
        log_error("TODO: loop until buffer is sent?");
    }
    return rtn;
}

int init_connected_socket(struct sockaddr_in *addr) {

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
    inet_ntop(AF_INET, &addr->sin_port, ip, INET_ADDRSTRLEN);
    int port = ntohs(addr->sin_port);
    if (connect(sock, (struct sockaddr*)addr, sizeof(*addr)) < 0) {
        log_perror("Failed to connect to socket at %s:%d", ip, port);
        close(sock);
        return -1;
    }

    log_custom(LOG_CONNECTIONS, "Connected socket to %s:%d", ip, port);
    return sock;
}

int init_bound_socket(int port) {
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

