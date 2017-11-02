#include "logging.h"
#include <inttypes.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#define MAX_EPOLL_EVENTS 512

/**
 * Enables a file descriptor which has already been aded to an epoll instance.
 * Or's EPOLLONESHOT with events so the event will only be responded to once
 */
int enable_epoll(int epoll_fd, int new_fd, uint32_t events) {
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.fd = new_fd;
    event.events = events | EPOLLONESHOT;

    int rtn = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, new_fd, &event);

    if (rtn == -1) {
        log(LOG_EPOLL_OPS, "failed to enable fd %d on epoll %d: %s",
                    new_fd, epoll_fd, strerror(errno));
        return -1;
    }
    log(LOG_EPOLL_OPS, "enabled fd %d on epoll", new_fd);
    return 0;
}

/**
 * Adds a file descriptor to the epoll instance.
 * Or's EPOLLONESHOT with events.
 */
int add_to_epoll(int epoll_fd, int new_fd, uint32_t events, bool oneshot) { 
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.fd = new_fd;
    if (oneshot) {
        event.events = events | EPOLLONESHOT;
    } else {
        event.events = events;
    }

    int rtn = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &event);

    if (rtn == -1) {
        log(LOG_EPOLL_OPS, "failed to add fd %d on epoll %d: %s",
                    new_fd, epoll_fd, strerror(errno));
        return -1;
    }
    log(LOG_EPOLL_OPS, "Added fd %d to epoll", new_fd);
    return 0;
}

/**
 * Accepts a new connection and adds it to the epoll instance
 */
static int accept_new_connection(int socketfd, int epoll_fd, int oneshot) {
    int rtn;
    log(LOG_EPOLL_OPS, "Accepting a new connection");

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int new_fd = accept(socketfd, (struct sockaddr*) &client_addr, &addr_len);
    if (new_fd == -1) {
        log_perror("Error accepting new connection from socket handler");
        return -1;
    }

#ifdef GET_NAME_INFO
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    rtn = getnameinfo((struct sockaddr*)&client_addr, addr_len,
                      hbuf, sizeof(hbuf),
                      sbuf, sizeof(sbuf),
                      NI_NUMERICHOST| NI_NUMERICSERV);
    if ( rtn == 0) {
        log(LOG_EPOLL_OPS, "Accepted connection on descriptor %d"
                                       "host=%s, port=%s", 
                   new_fd, hbuf, sbuf);
    }
#endif

    if (oneshot) {
        int flags = O_NONBLOCK;;
        rtn = fcntl(new_fd, F_SETFL, flags);
        if (rtn == -1) {
            log_perror("Error setting O_NONBLOCK");
            // TODO: Error handling :?
            return -1;
        }
    }
    rtn = add_to_epoll(epoll_fd, new_fd, EPOLLIN, oneshot);
    if (rtn < 0) {
        return -1;
    } else {
        return new_fd;
    }
}

/**
 * The main (blocking) loop for the socket handler.
 * Loops (epolling), accepting new connections and passing new
 * file descriptors to the next MSUs once data is available to be read
 * @param socket_fd Socket on which to accept new connections, or -1 if no accept is necessary
 * @param epoll_fd Fd of epoll instance
 * @param batch_size Number of connections to process in a row before exiting the loop or -1 for NA
 * @param timeout Epoll timeout. -1 for no timeout.
 * @param connection_handler Calls this function with args (int fd, void *data) on fd activity
 * @param accept_handler Calls this function with args (int fd, void *data) on new connection
 *                       NULL for no callback or N/A
 * @param data Data to be passed through to handler functions
 * @return 0 on success, -1 on error
 */
int epoll_loop(int socket_fd, int epoll_fd, int batch_size, int timeout, int oneshot,
       int (*connection_handler)(int, void*),
       int (*accept_handler)(int, void*),
       void *data) {
    struct epoll_event events[MAX_EPOLL_EVENTS];

    for (int j=0; j<batch_size || batch_size == -1;  j++) {
        log(LOG_EPOLL_OPS, "Waiting on epoll");
        int n = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, timeout);
        if (n == 0) {
            break;
        }
        log(LOG_EPOLL_OPS, "Epoll returned %d events", n);
        for (int i=0; i < n; ++i) {
            if (socket_fd == events[i].data.fd) {
                log(LOG_EPOLL_OPS, "Accepting connection on %d", socket_fd);
                int new_fd = accept_new_connection(socket_fd, epoll_fd, oneshot);
                if ( new_fd < 0) {
                    //log_error("Failed accepting new connection on epoll %d", epoll_fd);
                    return -1;
                } else {
                    if (accept_handler) {
                        accept_handler(new_fd, data);
                    }
                    log(LOG_EPOLL_OPS, "Accepted new connection");
                }
            } else {
                log(LOG_EPOLL_OPS, "Processing connection (fd: %d)",
                           events[i].data.fd);
                int rtn = connection_handler(events[i].data.fd, data);
                if (rtn != 0) {
                    if (rtn < 0) {
                        log_error("Failed processing existing connection (fd: %d)",
                                  events[i].data.fd);
                    } else {
                        log(LOG_EPOLL_OPS, "Got exit code %d from fd %d",
                                   rtn, events[i].data.fd);
                    }
                    return rtn;
                } else {
                    log(LOG_EPOLL_OPS, "Processed connection (fd: %d)",
                               events[i].data.fd);
                }
            }
        }
    }
    return 0;
}

/**
 * Initializes a new instance of an epoll file descriptor and adds a socket to it, listening
 * for input on that socket.
 * @param socket_fd Socket on which to listen for new connections.
 *                  -1 if creating epoll without socket.
 * @return epoll file descriptor
 */
int init_epoll(int socket_fd) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        log_perror("epoll_create1() failed");
        return -1;
    }

    if (socket_fd > 0) {
        struct epoll_event event;
        memset(&event, 0, sizeof(event));
        event.data.fd = socket_fd;
        event.events = EPOLLIN;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) == -1) {
            log_perror("epoll_ctl() failed to add socket");
            close(epoll_fd);
            return -1;
        }
    }
    log(LOG_EPOLL_OPS, "Created epoll fd %d",epoll_fd);
    return epoll_fd;
}
