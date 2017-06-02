/**
 * @file socket_handler_msu.c
 * MSU to handle network sockets
 */

// These guards around the headers are necessary if the module
// is to be implemented in c++ instead of C
#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>

#include "modules/socket_handler_msu.h"
#include "dedos_msu_list.h"
#include "runtime.h"
#include "modules/ssl_request_routing_msu.h"
#include "modules/ssl_msu.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h"
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
int socket_handler_receive(struct generic_msu *self, struct generic_msu_queue_item *queue_item) {
    struct socket_handler_state *state = self->internal_state;
    int ret, opt, i, n;

    n = epoll_wait(state->eventfd, state->events, MAX_EVENTS, 0);

    for (i = 0; i < n; ++i) {
        if ((state->events[i].events & EPOLLERR) ||
            (state->events[i].events & EPOLLHUP) ||
            (!(state->events[i].events & EPOLLIN))) {
            log_error("%s", "epoll error");
            //what if this is the main socket? Do we reboot the MSU?
            close(state->events[i].data.fd);
        } else if (state->socketfd == state->events[i].data.fd) {
            //main socket is receiving a new connection
            while (1) {
                log_debug("%s", "listening for new connections");
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int infd, flags;
                char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                addr_len = sizeof(client_addr);
                infd = accept(state->socketfd, (struct sockaddr *) &client_addr, &addr_len);
                if (infd == -1) {
                    if ((errno == EAGAIN) ||
                        (errno == EWOULDBLOCK)) {
                            //We have processed all incoming connections
                            //"EWOULDBLOCK means that the socket send buffer is full when sending,
                            //or that the socket receive buffer is empty when receiving."
                        break;
                    } else {
                        log_debug("%s", "accept failed");
                        break;
                    }
                }

                ret = getnameinfo(&client_addr, addr_len,
                                  hbuf, sizeof(hbuf),
                                  sbuf, sizeof(sbuf),
                                  NI_NUMERICHOST | NI_NUMERICSERV);
                if (ret == 0) {
                    log_debug("Accepted connection on descriptor %d (host=%s, port=%s)",
                              infd, hbuf, sbuf);
                }

                flags |= O_NONBLOCK;
                ret = fcntl(infd, F_SETFL, flags);
                if (ret == -1) {
                    log_error("%s", "error setting socket as O_NONBLOCK");
                    //close(state->socketfd) ?
                    return -1;
                }

                state->event.data.fd = infd;
                state->event.events = EPOLLIN | EPOLLET;
                ret = epoll_ctl(state->eventfd, EPOLL_CTL_ADD, infd, &state->event);
                if (ret == -1) {
                    log_error("%s", "epoll_ctl() failed");
                    return -1;
                }
            }
        } else {
            //Some data has to be read from an established connection
            log_debug("%s", "listening for data to be read on an established connection");
            switch (state->target_msu_type) {
                case DEDOS_SSL_READ_MSU_ID: {
                    int done = 0;
                    while (1) {
                        ssize_t count;
                        char buf[512];

                        /*
                        count = read(state->events[i].data.fd, buf, sizeof(buf));
                        if (count == -1) {
                            //errno == EAGAIN means that we've read all data
                            if (errno != EAGAIN) {
                                log_error("%s", "read failed");
                                done = 1;
                            }
                            break;
                        } else if (count == 0) {
                            //EOF. Remote end has closed the connection
                            done = 1;
                            break;
                        }
                        */
                        if (ssl_request_routing_msu == NULL) {
                            log_error("%s",
                                      "*ssl_request_routing_msu is NULL, forgot to create it?");
                            destroy_msu(self);
                        } else {
                            struct ssl_data_payload *data = malloc(sizeof(struct ssl_data_payload));
                            memset(data, '\0', MAX_REQUEST_LEN);
                            data->socketfd = state->events[i].data.fd;
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

                        //We dont' have a queue item as input so we already enqueue by ourself to the next MSU
                        return -10;
                    }
                }
                default:
                    debug("%s", "target MSU type for socket handler msu not recognized");
                    return -1;
            }
        }
    }

    //Because this MSU does not expect queue items to process, it has to wake up the
    //worker thread's queue somehow
    int thread_index = get_thread_index(pthread_self());
    struct dedos_thread *dedos_thread = &all_threads[thread_index];
    if (!pthread_equal(dedos_thread->tid, pthread_self())) {
        log_error("Unable to get correct pointer to self thread for socket handler msu init");
        return -1;
    }

    sem_post(dedos_thread->q_sem);

    return 0;
}

/**
 * Init an event structure for epolling
 * @param struct generic_msu *self: pointer to the msu instance
 * @param struct create_msu_thread_data *initial_state: contains init data
 * @return 0/-1 success/failure
 */
int socket_handler_init(struct generic_msu *self, struct create_msu_thread_data *initial_state) {
    int ret, opt, efd, flags;
    struct socket_handler_init_payload *init_data = initial_state->init_data;

    struct socket_handler_state *state = malloc(sizeof(struct socket_handler_state));
    self->internal_state = state;

    state->target_msu_type = init_data->target_msu_type;

    state->socketfd = socket(init_data->domain, init_data->type, init_data->protocol);
    opt = 1;
    if (setsockopt(state->socketfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1) {
        log_warn("setsockopt() failed with error %s (%d)", strerror(errno), errno);
    }

    flags |= O_NONBLOCK;
    ret = fcntl(state->socketfd, F_SETFL, flags);
    if (ret == -1) {
        log_error("%s", "error setting socket as O_NONBLOCK");
        //close(state->socketfd) ?
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = init_data->domain;
    addr.sin_addr.s_addr = init_data->bind_ip;
    addr.sin_port = htons(init_data->port);

    opt = 1;
    if (setsockopt(state->socketfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(int)) == -1) {
        log_error("%s", "Failed to set SO_REUSEPORT");
    }

    if (bind(state->socketfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        log_error("%s", "bind() failed");
        return -1;
    }

    ret = listen(state->socketfd, 5);
    if (ret == -1) {
        log_error("%s", "listen() failed");
        return -1;
    }

    state->eventfd = epoll_create1(0);
    if (efd == -1) {
        log_error("%s", "epoll_create1(0) failed");
        return -1;
    }

    state->event.data.fd = state->socketfd;
    state->event.events = EPOLLIN | EPOLLET;
    ret = epoll_ctl(state->eventfd, EPOLL_CTL_ADD, state->socketfd, &state->event);
    if (ret == -1) {
        log_error("%s", "epoll_ctl() failed");
        return -1;
    }

    state->events = calloc(MAX_EVENTS, sizeof(state->event));

    //Because this MSU does not expect queue items to process, it has to wake up the
    //worker thread's queue somehow
    int thread_index = get_thread_index(pthread_self());
    struct dedos_thread *dedos_thread = &all_threads[thread_index];
    if (!pthread_equal(dedos_thread->tid, pthread_self())) {
        log_error("Unable to get correct pointer to self thread for socket handler msu init");
        return -1;
    }

    sem_post(dedos_thread->q_sem);

    return 0;
}

/**
 * Close sockets handled by this MSU
 * @param: struct generic_msu self reference to this MSU
 * @param create_msu_thread_data initial_state reference to initial data structure
 * @return 0/-1 success/failure
 */
int socket_handler_destroy(struct generic_msu *self, struct create_msu_thread_data *initial_state) {
    struct socket_handler_state *state = self->internal_state;
    int ret;

    ret = close(state->socketfd);
    if (ret == - 1) {
        log_error("%s", "error closing socket");
        return ret;
    }

    return ret;
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
    .receive_ctrl=NULL,
    .route=shortest_queue_route,
    .send_local=default_send_local,
    .send_remote=default_send_remote,
    .deserialize=default_deserialize
};
