#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

int dedos_open(const char *path, int oflag) {
    return open(path, oflag);
}

int dedos_close(int fd) {
    return close(fd);
}

int dedos_socket(int d, int t, int p) {
    return socket(d, t, p);
}

int dedos_accept(int s, struct sockaddr *restrict a, socklen_t *restrict l) {
    return accept(s, a, l);
}
