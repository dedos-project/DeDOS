#include "communication.h"
#include "runtime_dfg.h"
#include "logging.h"

int send_to_endpoint(int fd, void *data, size_t data_len) {
    int rt_id = local_runtime_id();
    if (rt_id == -1) {
        log_error("Error getting source runtime ID");
        return -1;
    }
    struct dedos_tcp_msg *msg;
    size_t buflen = sizeof(*msg) + data_len;
    char buf[buflen];
    msg = (struct dedos_tcp_msg*) buf;
    msg->src_id = rt_id;
    msg->data_len = data_len;
    memcpy(&buf[sizeof(*msg)], data, data_len);
    int rtn = write(fd, buf, buflen);
    if (rtn <= 0) {
        log_error("Error sending buffer to endpoint with socket %d", fd);
    } else if (rtn < data_len) {
        log_error("Full buffer not sent to endpoint!");
        log_error("TODO: loop until buffer is sent?");
    }
    return rtn;
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

