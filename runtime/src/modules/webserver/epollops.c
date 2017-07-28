#include "logging.h"
#include <inttypes.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef LOG_EPOLL_OPS
#define LOG_EPOLL_OPS 0
#endif

#define MAX_EPOLL_EVENTS 512

static int last_epoll_fd = -1;

int enable_epoll(int epoll_fd, int new_fd, uint32_t events) {
    if (epoll_fd == 0) {
        epoll_fd = last_epoll_fd;
    }
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.fd = new_fd;
    event.events = events | EPOLLONESHOT;

    int rtn = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, new_fd, &event);

    if (rtn == -1) {
        log_perror("epoll_ctl() failed adding fd %d", new_fd);
        // TODO: Handle errors...
        return -1;
    }
    log_custom(LOG_EPOLL_OPS, "enabled fd %d on epoll", new_fd);
    return 0;
}

/**
 * Adds a file descriptor to the epoll instance so it can be responded to at a future time
 */
static int add_to_epoll(int epoll_fd, int new_fd, uint32_t events) {
    if (epoll_fd == 0) {
        epoll_fd = last_epoll_fd;
    }
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.fd = new_fd;
    event.events = events | EPOLLONESHOT;

    int rtn = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &event);

    if (rtn == -1) {
        log_perror("epoll_ctl() failed adding fd %d", new_fd);
        // TODO: Handle errors...
        return -1;
    }
    log_custom(LOG_EPOLL_OPS, "Added fd %d to epoll", new_fd);
    return 0;
}

/**
 * Accepts a new connection and adds it to the epoll instance
 */
static int accept_new_connection(int socketfd, int epoll_fd) {
    int rtn;
    log_custom(LOG_EPOLL_OPS, "Accepting a new connection");

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
        log_custom(LOG_EPOLL_OPS, "Accepted connection on descriptor %d"
                                       "host=%s, port=%s", 
                   new_fd, hbuf, sbuf);
    }
#endif

    int flags = O_NONBLOCK;
    rtn = fcntl(new_fd, F_SETFL, flags);
    if (rtn == -1) {
        log_perror("Error setting O_NONBLOCK");
        // TODO: Error handling :?
        return -1;
    }
    return add_to_epoll(epoll_fd, new_fd, EPOLLIN);
}

/**
 * The main (blocking) loop for the socket handler.
 * Loops (epolling) indefinitely, accepting new connections and passing new
 * file descriptors to the next MSUs once data is available to be read
 */
int epoll_loop(int socket_fd, int epoll_fd, 
       int (*connection_handler)(int, void*), void *data) {
    struct epoll_event events[MAX_EPOLL_EVENTS];

    while (1) {
        // Wait indefinitely
        int n = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);

        for (int i=0; i < n; ++i) {
            if (socket_fd == events[i].data.fd) {
                log_custom(LOG_EPOLL_OPS, "Accepting connection on %d", socket_fd);
                if (accept_new_connection(socket_fd, epoll_fd) != 0) {
                    log_error("Failed accepting new connection");
                } else {
                    log_custom(LOG_EPOLL_OPS, "Accepted new connection");
                }
            } else {
                log_custom(LOG_EPOLL_OPS, "Processing connection (fd: %d)", 
                           events[i].data.fd);
                if (connection_handler(events[i].data.fd, data) != 0) {
                    log_error("Failed processing existing connection (fd: %d)",
                              events[i].data.fd);
                } else {
                    log_custom(LOG_EPOLL_OPS, "Processed connection (fd: %d)",
                               events[i].data.fd);
                }
            }
        }
    }
    return 0;
}

int init_epoll(int socket_fd) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        log_perror("epoll_create1() failed");
        return -1;
    }
    last_epoll_fd = epoll_fd;

    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.fd = socket_fd;
    event.events = EPOLLIN;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event) == -1) {
        log_perror("epoll_ctl() failed to add socket");
        close(epoll_fd);
        return -1;
    }
    return epoll_fd;
}
