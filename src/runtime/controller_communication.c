/**
 * @file: controller_communication.c
 * Communication with global controller from runtime
 */

#include "controller_communication.h"
#include "communication.h"
#include "logging.h"
#include "socket_monitor.h"
#include "dedos_threads.h"
#include "thread_message.h"
#include "runtime_dfg.h"
#include "rt_stats.h"
#include "worker_thread.h"
#include "output_thread.h"

#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

/**
 * Static (global) variable to hold the socket
 * connecting to the global controller
 */
static int controller_sock = -1;


int send_to_controller(struct rt_controller_msg_hdr *hdr, void *payload) {

    if (controller_sock < 0) {
        log_error("Controller socket not initialized");
        return -1;
    }

    int rtn = send_to_endpoint(controller_sock, hdr, sizeof(*hdr));
    if (rtn <= 0) {
        log_error("Error sending header to controller");
        return -1;
    }
    if (hdr->payload_size <= 0) {
        return 0;
    }

    rtn = send_to_endpoint(controller_sock, payload, hdr->payload_size);
    if (rtn <= 0) {
        log_error("Error sending payload to controller");
        return -1;
    }

    log(LOG_CONTROLLER_COMMUNICATION, "Sent payload of size %d type %d to controller",
               (int)hdr->payload_size, hdr->type);
    return 0;
}

/**
 * Sends the initilization message containing runtime ID, ip and port to
 * global controller
 */
static int send_ctl_init_msg() {
    int local_id = local_runtime_id();
    if (local_id < 0) {
        log_error("Could not get local runtime ID to send to controller");
        return -1;
    }
    uint32_t ip = local_runtime_ip();
    int port = local_runtime_port();

    struct rt_controller_init_msg msg = {
        .runtime_id = local_id,
        .ip = ip,
        .port = port
    };

    struct rt_controller_msg_hdr hdr = {
        .type = RT_CTL_INIT,
        .payload_size = sizeof(msg)
    };

    return send_to_controller(&hdr, &msg);
}

/**
 * Initializes a connection to the global controller
 * @returns 0 on success, -1 on error
 */
static int connect_to_controller(struct sockaddr_in *addr) {

    if (controller_sock != -1) {
        log_error("Controller socket already initialized");
        return -1;
    }

    controller_sock = init_connected_socket(addr);

    if (controller_sock < 0) {
        log_error("Error connecting to global controller!");
        return -1;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
    int port = ntohs(addr->sin_port);

    log_info("Connected to global controller at %s:%d", ip, port);

    int rtn = send_ctl_init_msg();
    if (rtn < 0) {
        log_error("Error sending initialization message to controller");
        return -1;
    }
    return controller_sock;
}

/**
 * Macro to check whether the size of a message matches the size
 * of the struct it's supposed to be
 */
#define CHECK_MSG_SIZE(msg, target) \
    if (msg->payload_size != sizeof(target)) { \
        log_warn("Message data size (%d) does not match size" \
                 "of target type (%d)" #target, (int)msg->payload_size , \
                 (int)sizeof(target)); \
        return -1; \
    } \
    return 0;

/**
 * Checks whether the size of a message matches the size of its target struct
 */
static int verify_msg_size(struct ctrl_runtime_msg_hdr *msg) {
    switch (msg->type) {
        case CTRL_CONNECT_TO_RUNTIME:
            CHECK_MSG_SIZE(msg, struct ctrl_add_runtime_msg);
        case CTRL_CREATE_THREAD:
            CHECK_MSG_SIZE(msg, struct ctrl_create_thread_msg);
        case CTRL_DELETE_THREAD:
            CHECK_MSG_SIZE(msg, struct ctrl_create_thread_msg);
        case CTRL_MODIFY_ROUTE:
            CHECK_MSG_SIZE(msg, struct ctrl_route_msg);
        case CTRL_CREATE_MSU:
            CHECK_MSG_SIZE(msg, struct ctrl_create_msu_msg);
        case CTRL_DELETE_MSU:
            CHECK_MSG_SIZE(msg, struct ctrl_delete_msu_msg);
        case CTRL_MSU_ROUTES:
            CHECK_MSG_SIZE(msg, struct ctrl_msu_route_msg);
        default:
            log_error("Received unknown message type: %d", msg->type);
            return -1;
    }
}

/**
 * Processes a received ctrl_add_runtime_msg
 */
static int process_connect_to_runtime(struct ctrl_add_runtime_msg *msg) {
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = msg->ip;
    addr.sin_port = htons(msg->port);

    int rtn = connect_to_runtime_peer(msg->runtime_id, &addr);
    if (rtn < 0) {
        log_error("Could not add runtime peer");
        return -1;
    }
    return 0;
}

/**
 * Processes a received ctrl_create_thread_msg
 */
static int process_create_thread_msg(struct ctrl_create_thread_msg *msg) {
    int id = msg->thread_id;
    int rtn = create_worker_thread(id, msg->mode);
    if (rtn < 0) {
        log_error("Error creating worker thread %d", id);
        return -1;
    }
    log(LOG_THREAD_CREATION, "Created worker thread %d", id);
    return 0;
}

/**
 * Processes a received ctrl_route_msg
 */
static int process_ctrl_route_msg(struct ctrl_route_msg *msg) {
    int rtn;
    log(LOG_CONTROLLER_COMMUNICATION, "Got control route message of type %d", msg->type);
    switch (msg->type) {
        case CREATE_ROUTE:
            rtn = init_route(msg->route_id, msg->type_id);
            if (rtn < 0) {
                log_error("Error creating new route of id %d, type %d",
                          msg->route_id, msg->type_id);
                return 1;
            }
            return 0;
        case ADD_ENDPOINT:;
            struct msu_endpoint endpoint;
            int rtn = init_msu_endpoint(msg->msu_id, msg->runtime_id, &endpoint);
            if (rtn < 0) {
                log_error("Cannot initilize runtime endpoint for adding "
                          "endpoint %d to route %d", msg->msu_id, msg->route_id);
                return 1;
            }
            rtn = add_route_endpoint(msg->route_id, endpoint, msg->key);
            if (rtn < 0) {
                log_error("Error adding endpoint %d to route %d with key %d",
                          msg->msu_id, msg->route_id, msg->key);
                return 1;
            }
            return 0;
        case DEL_ENDPOINT:
            rtn = remove_route_endpoint(msg->route_id, msg->msu_id);
            if (rtn < 0) {
                log_error("Error removing endpoint %d from route %d",
                          msg->msu_id, msg->route_id);
                return 1;
            }
            return 0;
        case MOD_ENDPOINT:
            rtn = modify_route_endpoint(msg->route_id, msg->msu_id, msg->key);
            if (rtn < 0) {
                log_error("Error modifying endpoint %d on route %d to have key %d",
                          msg->msu_id, msg->route_id, msg->key);
                return 1;
            }
            return 0;
        default:
            log_error("Unknown route control message type received: %d", msg->type);
            return -1;
    }
}

/**
 * Gets the corresponding thread_msg_type for a ctrl_runtime_msg_type
 */
static enum thread_msg_type get_thread_msg_type(enum ctrl_runtime_msg_type type) {
    switch (type) {
        case CTRL_CREATE_MSU:
            return CREATE_MSU;
        case CTRL_DELETE_MSU:
            return DELETE_MSU;
        case CTRL_MSU_ROUTES:
            return MSU_ROUTE;
        default:
            log_error("Unknown thread message type %d", type);
            return UNKNOWN_THREAD_MSG;
    }
}

/**
 * Constructs a thread message from a ctrl_runtime_msg_hdr, reading any additional
 * information it needs off of the associated socket
 * @param hdr Header describing the information available to read
 * @param fd The file descriptor off of which to read the remainder of the control message
 * @return Created thread_msg on success, NULL on error
 */
static struct thread_msg *thread_msg_from_ctrl_hdr(struct ctrl_runtime_msg_hdr *hdr, int fd) {
    if (verify_msg_size(hdr) != 0) {
        log_warn("May not process message. Incorrect payload size for type.");
    }

    void *msg_data = malloc(hdr->payload_size);
    int rtn = read_payload(fd, hdr->payload_size, msg_data);
    if (rtn < 0) {
        log_error("Error reading control payload. Cannot process message");
        free(msg_data);
        return NULL;
    }
    log(LOG_CONTROLLER_COMMUNICATION, "Read control payload %p of size %d",
               msg_data, (int)hdr->payload_size);
    enum thread_msg_type type = get_thread_msg_type(hdr->type);
    struct thread_msg *thread_msg = construct_thread_msg(type, hdr->payload_size, msg_data);
    thread_msg->ack_id = hdr->id;
    return thread_msg;
}

/**
 * Constructs a thread_msg from a control-runtime message and passes it to the relevant thread.
 * @param hdr Header describing info available to read
 * @param fd File descriptor to read message from
 * @return 0 on success, -1 on error
 */
static int pass_ctrl_msg_to_thread(struct ctrl_runtime_msg_hdr *hdr, int fd) {
    struct thread_msg *thread_msg = thread_msg_from_ctrl_hdr(hdr, fd);
    if (thread_msg == NULL) {
        log_error("Error constructing thread msg from control hdr");
        return -1;
    }
    struct dedos_thread *thread = get_dedos_thread(hdr->thread_id);
    if (thread == NULL) {
        log_error("Error getting dedos thread %d to deliver control message",
                  hdr->thread_id);
        destroy_thread_msg(thread_msg);
        return -1;
    }

    int rtn = enqueue_thread_msg(thread_msg, &thread->queue);
    if (rtn < 0) {
        log_error("Error enquing control message on thread %d", hdr->thread_id);
        return -1;
    }
    return 0;
}

/**
 * Processes a received control message that is due for delivery to this thread.
 * @param hdr The header for the control message
 * @param fd File descriptor off of which to read the control message
 * @return 0 on success, -1 on error
 */
static int process_ctrl_message(struct ctrl_runtime_msg_hdr *hdr, int fd) {
    if (verify_msg_size(hdr) != 0) {
        log_warn("May not process message. Incorrect payload size for type");
    }

    char msg_data[hdr->payload_size];
    int rtn = read_payload(fd, hdr->payload_size, (void*)msg_data);

    if (rtn < 0) {
        log_error("Error reading control payload. Cannot process message");
        return -1;
    }
    log(LOG_CONTROLLER_COMMUNICATION, "Read control payload %p of size %d",
               msg_data, (int)hdr->payload_size);

    switch (hdr->type) {
        case CTRL_CONNECT_TO_RUNTIME:
            rtn = process_connect_to_runtime((struct ctrl_add_runtime_msg*) msg_data);
            if (rtn < 0) {
                log_error("Error processing connect to runtime message");
                return -1;
            }
            break;
        case CTRL_CREATE_THREAD:
            rtn = process_create_thread_msg((struct ctrl_create_thread_msg*) msg_data);
            if (rtn < 0) {
                log_error("Error processing create thread message");
                return -1;
            }
            break;
        case CTRL_DELETE_THREAD:
            log_critical("TODO!");
            break;
        case CTRL_MODIFY_ROUTE:
            rtn = process_ctrl_route_msg((struct ctrl_route_msg*) msg_data);
            if (rtn < 0) {
                log_error("Error processing control route message");
                return -1;
            }
            break;
        default:
            log_error("Unknown message type delivered to input thread: %d", hdr->type);
            return -1;
    }
    return 0;
}

//TODO: send_ack_message()
int send_ack_message(int id, bool success) {
    // Not implemented yet
    return 0;
}

/** Processes any received control message. */
static int process_ctrl_message_hdr(struct ctrl_runtime_msg_hdr *hdr, int fd) {

    int rtn;
    switch (hdr->type) {
        case CTRL_CREATE_MSU:
        case CTRL_DELETE_MSU:
        case CTRL_MSU_ROUTES:
            rtn = pass_ctrl_msg_to_thread(hdr, fd);
            if (rtn < 0) {
                log_error("Error passing control message to thread");
                return -1;
            }
            break;
        case CTRL_CONNECT_TO_RUNTIME:
        case CTRL_CREATE_THREAD:
        case CTRL_DELETE_THREAD:
        case CTRL_MODIFY_ROUTE:
            rtn = process_ctrl_message(hdr, fd);
            if (rtn < 0) {
                log_error("Error processing control message");
                send_ack_message(hdr->id, false);
                return -1;
            }
            send_ack_message(hdr->id, true);
            break;
        default:
            log_error("Unknown header type %d in receiving thread", hdr->type);
            return -1;
    }

    return 0;
}

int handle_controller_communication(int fd) {
    struct ctrl_runtime_msg_hdr hdr;
    int rtn = read_payload(fd, sizeof(hdr), &hdr);
    if (rtn< 0) {
        log_error("Error reading control message");
        return -1;
    } else if (rtn == 1) {
        log_critical("Disconnected from global controller");
        close(fd);
        return 1;
    } else {
        log(LOG_CONTROLLER_COMMUNICATION,
                   "Read header (type %d) from controller", hdr.type);
    }

    rtn = process_ctrl_message_hdr(&hdr, fd);
    if (rtn < 0) {
        log_error("Error processing control message");
        return -1;
    }

    return 0;
}

bool is_controller_fd(int fd) {
    return fd == controller_sock;
}


int init_controller_socket(struct sockaddr_in *addr) {
    int sock = connect_to_controller(addr);
    if (sock < 0) {
        log_error("Error connecting to global controller");
        return -1;
    }
    if (monitor_controller_socket(sock) != 0) {
        log_error("Attempted to initialize controller socket "
                  "before initializing runtime epoll");
        return -1;
    }
    return sock;
}

int send_stats_to_controller() {
    if (controller_sock < 0) {
        log(LOG_STAT_SEND, "Skipping sending statistics: controller not initialized");
        return -1;
    }
    int rtn = 0;
    int total_items = 0;
    for (int i=0; i<N_REPORTED_STAT_TYPES; i++) {
        enum stat_id stat_id = reported_stat_types[i].id;
        int n_items;
        struct stat_sample *samples = get_stat_samples(stat_id, &n_items);
        total_items += n_items;
        if (samples == NULL) {
            log(LOG_STAT_SEND, "Error getting stat sample for send to controller");
            continue;
        }
        size_t serial_size = serialized_stat_sample_size(samples, n_items);

        char buffer[serial_size];
        size_t ser_rtn = serialize_stat_samples(samples, n_items, buffer, serial_size);
        if (ser_rtn < 0) {
            log_error("Error serializing stat sample");
            rtn = -1;
        }

        struct rt_controller_msg_hdr hdr = {
            .type = RT_STATS,
            .payload_size = ser_rtn
        };

        int rtn = send_to_controller(&hdr, buffer);
        if (rtn < 0) {
            log_error("Error sending statistics to controller");
            rtn = -1;
        }
    }
    log(LOG_STAT_SEND, "Sending %d statistics to controller", total_items);
    return rtn;
}
