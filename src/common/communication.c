/**
 * @file communication.c
 *
 * General-purpose socket communication functions used from
 * global controller, runtime, or MSUs
 */
#include "communication.h"
#include "logging.h"

#include <arpa/inet.h>

/**
 * The maximum number of times that a call to `read()`
 * can be attempted for a single buffer before giving up
 */
#define MAX_READ_ATTEMPTS 100

/**
 * Reads a buffer of a given size from a file descriptor.
 * Loops until that many bytes have been read off of the socket, until the socket is closed,
 * or until it has tried more than `MAX_READ_ATTEMPTS` times.
 * @param fd The file descriptor from which to read
 * @param size The number of bytes to read off of the file desriptor
 * @param buff The buffer into which the read the bytes
 * @returns 0 on success, -1 on error
 */
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

/**
 * Writes a buffer of a given size to a file descriptor.
 * Loops on the write call until all of the bytes have been sent or it encounters an error.
 * @param fd The file descriptor to which the data is to be written
 * @param data The buffer to write to the file descriptor
 * @param data_len The size of the buffer to write
 * @returns Number of bytes writen. 0 if no bytes could be written.
 */
ssize_t send_to_endpoint(int fd, void *data, size_t data_len) {
    ssize_t size = 0;
    while (size < data_len) {
        ssize_t rtn = write(fd, data + size, data_len - size);
        if (rtn <= 0) {
            log_error("Error sending buffer to endpoint with socket %d", fd);
            break;
        } else if (rtn < data_len) {
            log_warn("Full buffer not sent to endpoint!");
        }
        size += rtn;
    }
    return size;
}

/**
 * Initializes a socket that is connected to a given address.
 * Blocks until the connection has been estabslished.
 * Sets port and address to be reusable
 * @param addr The address to connect to
 * @returns file descriptor on sucecss, -1 on error
 */
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

/**
 * Initializes a socket which is bound to a given port (and any local IP address).
 * Sets `REUSEPORT` and `REUSEADDR` on the socket.
 * @param port The port to which to bind
 * @returns the file descriptor on success, -1 on error
 */
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

/**
 * The backlog size for listening sockets
 */
#define BACKLOG 1024

/**
 * Initializes a socket which is bound to and listening on the given port
 * @param port The port on which to listen
 * @return The file descriptor on success, -1 on error
 */
int init_listening_socket(int port) {
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
