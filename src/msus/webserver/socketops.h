/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
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
