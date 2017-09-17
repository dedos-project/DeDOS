
struct runtime_endpoint {
    int fd;
    uint32_t ip;
    int port;
};

#define MAX_RUNTIME_ID 32

static struct runtime_endpoint runtime_endpoints[MAX_RUNTIME_ID];

int send_to_runtime(unsigned int runtime_id, struct ctrl_runtime_msg_hdr *hdr, void *payload) {
    if (runtime_id > MAX_RUNTIME_ID) {
        log_error("Requested runtime %d is greater than max runtime ID %d",
                  runtime_id, MAX_RUNTIME_ID);
        return -1;
    }
    struct runtime_endpoint *endpoint = &runtime_endpoints[runtime_id];
    if (endpoint->fd <= 0) {
        log_error("Requested runtime %d not instantiated", runtime_id);
        return -1;
    }

    int rtn = send_to_endpoint(endpoint->fd, hdr, sizeof(*hdr));
    if (rtn <= 0) {
        log_error("Error sending header to runtime %d", runtime_id);
        return -1;
    }

    if (hdr->payload_size <= 0) {
        return 0;
    }

    rtn = send_to_endpoint(endpoint->fd, payload, hdr->payload_size);
    if (rtn <= 0) {
        log_error("Error sending payload to runtime %d", runtime_id);
        return -1;
    }
    log_custom(LOG_RUNTIME_SENDS, "Sent a payload of size %d to runtime %d (fd: %d)",
               (int)hdr->payload_size, runtime_id, endpoint->fd);
    return 0;
}

static int send_add_runtime_msg(unsigned int target_id, int new_rt_id,
                                uint32_t ip, int port) {
    struct ctrl_add_runtime_msg msg = {
        .runtime_id = new_rt_id,
        .ip = ip,
        .port = port
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_CONNECT_TO_RUNTIME,
        .thread_id = 0,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_runtime(target_id, &hdr, &msg);
    if (rtn < 0) {
        log_error("Error sending initialization message for rt %d to rt %d",
                  new_rt_id, target_id);
    } else {
        log_custom(LOG_RT_COMMUNICATION, "Send initialization message for rt %d to rt %d",
                   new_rt_id, target_id);
    }
    return 0;
}

static int add_runtime_endpoint(unsigned int runtime_id, int fd, uint32_t ip, int port) {
    struct stat buf;
    if (fstat(fd, &buf) != 0) {
        log_error("Cannot register non-descriptor %d for runtime ID %d", fd, runtime_id);
        return -1;
    }
    if (runtime_endpoints[runtime_id].fd != 0) {
        log_warn("Replacing runtime peer with id %d", runtime_id);
    }
    runtime_endpoints[runtime_id].fd = fd;

    for (int i=0; i<MAX_RUNTIME_ID; i++) {
        if (i != runtime_id && runtime_endpoints[runtime_id].fd != 0) {
            if (send_add_runtime_msg(i, runtime_id, ip, port) != 0) {
                log_error("Failed to add runtime %d to runtime %d",
                          runtime_id, i);
            }
        }
    }

    return 0;
}

static int read_rt_msg_hdr(int fd, struct rt_controller_msg_hdr *hdr) {
    ssize_t rtn = read(fd, (void*)hdr, sizeof(*hdr));
    if (rtn != sizeof(*hdr)) {
        log_error("Could not read full runtiem message from socket %d", fd);
        return -1;
    }
    return rtn;
}

static int process_rt_init_message(ssize_t payload_size, int fd) {
    struct rt_controller_init_msg msg;
    if (payload_size != sizeof(msg)) {
        log_error("Payload size does not match size of runtime initialization message");
        return -1;
    }
    int rtn = read_payload(fd, sizeof(msg), &msg);
    if (rtn < 0) {
        log_error("Error reading runtime init payload. Cannot process message");
        return -1;
    }

    rtn = add_runtime_endpoint(msg.runtime_id, fd);
    if (rtn < 0) {
        log_error("Error adding runtime endpoint");
        return -1;
    }
    return 0;
}

static int process_rt_stats_message(size_t payload_size, int fd) {

    char buffer[payload_size];
    int rtn = read_payload(fd, sizeof(buffer), &buffer);
    if (rtn < 0) {
        log_error("Error reading stats message payload");
        return -1;
    }
    return handle_serialized_stats_buffer(buffer, payload_size);
}

static int process_rt_message_hdr(struct rt_controller_msg_hdr *hdr, int fd) {
    int rtn;
    switch (hdr->type) {
        case RT_CTL_INIT:
            rtn = process_rt_init_message(hdr->payload_size, fd);
            if (rtn < 0) {
                log_error("Erorr processing init runtime message from fd %d", fd);
                return -1;
            }
            return 0;
        case RT_STATS:
            rtn = process_rt_stats_message(hdr->payload_size, fd);
            if (rtn < 0) {
                log_error("Error processing rt stats message from fd %d", fd);
                return -1;
            }
            return 0;
        default:
            log_error("Received unknown message type from fd %d: %d", fd, hdr->type);
    }
}

int handle_runtime_communication(int fd) {
    struct rt_controller_msg_hdr hdr;
    ssize_t msg_size = read_rt_msg_hdr(fd, &hdr);
    if (msg_size < 0) {
        log_error("Error reading runtime message header");
        return -1;
    } else {
        log_custom(LOG_RT_COMMUNICATION,
                   "read message from runtime of size %d", (int)msg_size);
    }

    int rtn = process_rt_message_hdr(&hdr, fd);
    if (rtn < 0) {
        log_error("Error processing rt message");
        return -1;
    }
    return 0;
}
