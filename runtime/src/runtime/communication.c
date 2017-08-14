
static int epoll_fd = -1;

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
    mdg->src_id = rt_id;
    msg->data_len = data_len;
    memcpy(&buf[sizeof(*msg)], data, data_len);
    int rtn = send(fd, buf, buflen);
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
}


int init_runtime_epoll() {
    if (epoll_fd != -1) {
        log_error("runtime epoll already initialized");
        return -1;
    }
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        log_perror("Error creating epoll instance");
        return -1;
    }
    return 0;
}

#define MAX_EPOLL_FD 32
enum fd_handler fd_handlers[MAX_EPOLL_FD];

int add_to_runtime_epoll(int fd, enum fd_handler handler) {
    if (epoll_fd < 0) {
        log_error("Runtime epoll not initialized");
        return -1;
    }

    if (fd > MAX_EPOLL_FD) {
        log_error("Cannot add fd %d to runtime epoll. Maximum FD is %d", MAX_EPOLL_FD);
        return -1;
    }

    fd_handlers[fd] = handler;

    struct epoll_event event = {
        .event = EPOLLIN
    };
    event.data.fd = fd;

    if (epollctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        log_perror("Failed to add fd %d to epoll", fd);
        return -1;
    }
    return 0;
}

#define MAX_EPOLL_EVENTS 128

int check_runtime_epoll(bool blocking) {
    if (epoll_fd < 0) {
        log_error("Runtime epoll not initialized");
        return -1;
    }
    
    struct epoll_event events[MAX_EPOLL_EVENTS];
    int n_events;
    if (blocking) {
        n_events = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);
    } else {
        n_events = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, 0);
    }
    int handled_events = 0;
    int rtn;
    for (int i=0; i<n_events; i++) {
        struct epoll_event *event = &events[i];
        int fd = event->data.fd;
        switch (fd_handlers[fd]) {
            case RUNTIME_HANDLER:
                rtn = handle_runtime_communication(fd);
                if (rtn < 0) {
                    return rtn;
                }
                handled_events++;
                break;
            case CONTROLLER_HANDLER:
                rtn = handle_controller_communication(fd);
                if (rtn < 0) {
                    return rtn;
                }
                handled_events++;
                break;
            default:
                log_error("Undefined handler for fd %d", fd);
                return -1;
        }
    }
    return handled_events;
}
