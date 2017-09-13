#include "webserver/socketops.h"
#include "webserver/webserver.h"
#include <unistd.h>
#include "logging.h"
#include <netinet/ip.h>


static struct sock_settings ws_sock_settings =  {
    .domain = AF_INET,
    .type = SOCK_STREAM,
    .protocol = 0,
    .bind_ip = INADDR_ANY,
    .reuse_addr = 1,
    .reuse_port = 1
};

struct sock_settings *webserver_sock_settings(int port) {
    ws_sock_settings.port = port;
    return &ws_sock_settings;
}

#define SOCKET_BACKLOG 512

int init_socket(struct sock_settings *settings) {

    int socket_fd = socket(
            settings->domain,
            settings->type,
            settings->protocol);

    if ( socket_fd == -1 ) {
        log_perror("socket() failed");
        return -1;
    }

    int opt = settings->reuse_addr;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        log_perror("setsockopt() failed for reuseaddr");
    }
    opt = settings->reuse_port;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(&opt)) == -1) {
        log_perror("setsockopt() failed for reuseport");
    }

    struct sockaddr_in addr;
    addr.sin_family = settings->domain;
    addr.sin_addr.s_addr = settings->bind_ip;
    addr.sin_port = htons(settings->port);
 
    if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        log_perror("bind() failed");
        close(socket_fd);
        return -1;
    }

    if (listen(socket_fd, SOCKET_BACKLOG) == -1) {
        log_perror("listen() failed");
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

int read_socket(int fd, char *buf, int *buf_size) {
    int num_bytes = read(fd, buf, *buf_size);
    if (num_bytes >= 0) {
        *buf_size = num_bytes;
        return WS_COMPLETE;
    } else {
        if (errno == EAGAIN) {
            return WS_INCOMPLETE_READ;
        } else {
            log_perror("Error reading from socket %d", fd);
            return WS_ERROR;
        }
    }
}

int write_socket(int fd, char *buf, int *buf_size) {
    int num_bytes = write(fd, buf, *buf_size);
    if (num_bytes > 0) {
        if (num_bytes != *buf_size) {
            log_warn("Didn't write the right number of bytes?");
        }
        *buf_size = num_bytes;
        return WS_COMPLETE;
    } else {
        if (errno == EAGAIN) {
            return WS_INCOMPLETE_WRITE;
        } else {
            log_perror("Error writing to socket %d (rtn: %d, requested: %d)", 
                    fd, num_bytes, *buf_size);
            return WS_ERROR;
        }
    }
}
