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
#ifndef SSL_OPS_H_
#define SSL_OPS_H_
#include <openssl/ssl.h>

void init_ssl_locks(void);
void kill_ssl_locks(void);

int init_ssl_context(void);
int load_ssl_certificate(char *cert_file, char *key_file);

SSL *init_ssl_connection(int fd);
int accept_ssl(SSL *ssl);
int read_ssl(SSL *ssl, char *buf, int *buf_size);
int write_ssl(SSL *ssl, char *buf, int *buf_size);
int close_ssl(SSL *ssl);

#endif
