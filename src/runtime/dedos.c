#include "rt_stats.h"
#include "logging.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int current_thread_id();
int current_msu_id();

#define MAX_FDS 131072
static int thread_fds[MAX_FDS];
static int msu_fds[MAX_FDS];

void increment_fdcount(int fd) {
    increment_rt_stat(FILEDES, 1);
    int id;
    if ( (id = current_thread_id()) != -1) {
        thread_fds[fd] = id;
        if (increment_thread_stat(FILEDES, id, 1)) {
            log_error("Error incrementhing thread stat %d", id);
        }
    }
    if ( (id = current_msu_id()) != -1) {
        msu_fds[fd] = id;
        increment_msu_stat(FILEDES, id, 1);
    }
}

void decrement_fdcount(int fd) {
    increment_rt_stat(FILEDES, -1);
    if (thread_fds[fd] != 0) {
        increment_thread_stat(FILEDES, thread_fds[fd], -1);
        thread_fds[fd] = 0;
    }
    if (msu_fds[fd] != 0) {
        increment_msu_stat(FILEDES, msu_fds[fd], -1);
        msu_fds[fd] = 0;
    }
}

int dedos_open(const char *path, int oflag) {
    int rtn = open(path, oflag);
    if (rtn > 0) {
        increment_fdcount(rtn);
    }
    return rtn;
}


int dedos_close(int fildes) {
    decrement_fdcount(fildes);
    int rtn = close(fildes);
    if (rtn != 0) {
        increment_fdcount(fildes);
    }
    return rtn;
}

int dedos_socket(int domain, int type, int protocol) {
    int rtn = socket(domain, type, protocol);
    if (rtn > 0) {
        increment_fdcount(rtn);
    }
    return rtn;
}

int dedos_accept(int socket, struct sockaddr *restrict address, socklen_t *restrict address_len) {
    int rtn = accept(socket, address, address_len);
    if (rtn > 0) {
        increment_fdcount(rtn);
    }
    return rtn;
}


