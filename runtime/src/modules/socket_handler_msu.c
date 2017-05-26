/**
 * @file dummy_msu.c
 * Example usage of MSU - not to be instantiated.
 */

// These guards around the headers are necessary if the module
// is to be implemented in c++ instead of C
#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <errno.h>

#include "modules/socket_handler_msu.h"
#include "dedos_msu_list.h"
#include "runtime.h"
#include "modules/ssl_request_routing_msu.h"
#include "modules/ssl_msu.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"

#ifdef __cplusplus
}
#endif

/**
 * poll on a given list of file descriptors, and forward data to some MSU
 * @param self: MSU instance receiving data
 * @param queue_item: queue item to be received
 * @return Type-id of MSU to receive data next, -1 on error, and 0 if no packet was read.
 */
int socket_handler_receive(struct generic_msu *self, msu_queue_item *queue_item) {
    struct socket_handler_state *state = self->internal_state;
    struct pollfd fd[1];
    fd[0].fd = state->socketfd;
    fd[0].events = POLLIN;

    int ret = poll(fd, 1, 0);

    if (ret == 1) {
        log_error("%s", "Error in Poll");
    } else if (ret != 0 && fd[0].revents & POLLIN) {
        switch (state->target_msu_type) {
            case DEDOS_SSL_READ_MSU_ID: {
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int new_sock = accept(state->socketfd,
                                      (struct sockaddr *) &client_addr,
                                      &addr_len);
                if (new_sock < 0) {
                    log_warn("accept(%d,...) failed with error %s",
                             state->socketfd,
                             strerror(errno));
                    return -1;
                }

                struct timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 10000;

                if (setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO,
                               (char *) &timeout, sizeof(struct timeval)) == 1) {
                    log_error("setsockopt(%d,...) failed with error %s", new_sock, strerror(errno));
                    return -1;
                }

                if (ssl_request_routing_msu == NULL) {
                    log_error("*ssl_request_routing_msu is NULL, forgot to create it?%s","");
                    destroy_msu(self);
                } else {
                    struct ssl_data_payload *data = malloc(sizeof(struct ssl_data_payload));
                    memset(data, '\0', MAX_REQUEST_LEN);
                    data->socketfd = new_sock;
                    data->type = READ;
                    data->state = NULL;

                    struct generic_msu_queue_item *new_item_ws =
                        malloc(sizeof(struct generic_msu_queue_item));
                    new_item_ws->buffer = data;
                    new_item_ws->buffer_len = sizeof(struct ssl_data_payload);
#ifdef DATAPLANE_PROFILING
                    new_item_ws->dp_profile_info.dp_id = get_request_id();
#endif
                    int ssl_request_queue_len =
                        generic_msu_queue_enqueue(&ssl_request_routing_msu->q_in, new_item_ws);

                    if (ssl_request_queue_len < 0) {
                        log_error("Failed to enqueue ssl_request to %s",
                                  ssl_request_routing_msu->type->name);
                        free(new_item_ws);
                        free(data);
                    } else {
                        log_debug("Enqueued ssl forwarding request, q_len: %u",
                                  ssl_request_queue_len);
                    }
                }

                return DEDOS_SSL_READ_MSU_ID;
            }
            default:
                debug("%s", "target MSU type for socket handler msu not recognized");
                return -1;
        }
    }

    return 0;
}

//add or remove fd to list
int socket_handler_receive_ctrl(struct generic_msu *msu, msu_queue_item *queue_item) {
}

/*
* Init a socket to listen to
* @param struct generic_msu *self: pointer to the msu instance
* @param struct create_msu_thread_msg_data *initial_state: contains init data
**/
int socket_handler_init(struct generic_msu *self, struct create_msu_thread_msg_data *initial_state) {
    struct socket_handler_init_payload *init_data = initial_state->creation_init_data;

    struct socket_handler_state *state = malloc(sizeof(struct socket_handler_state));
    self->internal_state = state;

    state->target_msu_type = init_data->target_msu_type;

    state->socketfd = socket(init_data->domain, init_data->type, init_data->protocol);
    int opt = 1;
    if (setsockopt(state->socketfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1) {
        log_warn("setsockopt() failed with error %s (%d)", strerror(errno), errno);
    }

    struct sockaddr_in addr;
    addr.sin_family = init_data->domain;
    addr.sin_addr.s_addr = init_data->bind_ip;
    addr.sin_port = htons(init_data->port);

    opt = 1;
    if (setsockopt(state->socketfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(int)) < 0) {
        log_error("%s", "Failed to set SO_REUSEPORT");
    }

    if (bind(state->socketfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        log_error("%s", "bind() failed");
        return -1;
    }

    listen(state->socketfd, 5);

    free(init_data);

    return 0;
}

//close all sockets
int socket_handler_destroy(struct generic_msu *self, struct create_msu_thread_msg_data *initial_state) {
    struct socket_handler_state *state = self->internal_state;
    close(state->socketfd);
}

/**
 * Keep track of a set of file descriptors and poll them
 */
struct msu_type SOCKET_HANDLER_MSU_TYPE = {
    .name="socket_handler_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_SOCKET_HANDLER_MSU_ID,
    .proto_number=0,
    .init=socket_handler_init,
    .destroy=socket_handler_destroy,
    .receive=socket_handler_receive,
    .receive_ctrl=socket_handler_receive_ctrl,
    .route=round_robin,
    .send_local=default_send_local,
    .send_remote=default_send_remote,
    .deserialize=default_deserialize
};
