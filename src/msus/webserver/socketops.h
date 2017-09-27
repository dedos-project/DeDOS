#ifndef SOCKET_OPS_H_
#define SOCKET_OPS_H_

struct sock_settings {
    int port;
    int domain;
    int type;
    int protocol;
    unsigned long bind_ip;
    int reuse_addr;
    int reuse_port;
};

struct sock_settings *webserver_sock_settings(int port);

int init_socket(struct sock_settings *settings);
int read_socket(int fd, char *buf, int *buf_size);
int write_socket(int fd, char *buf, int *buf_size);
#endif
