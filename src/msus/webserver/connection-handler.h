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
#ifndef CONNECTION_HANDLER_H_
#define CONNECTION_HANDLER_H_

#include <openssl/ssl.h>
#include "webserver.h"
#include "dbops.h"
#include "request_parser.h"

#define MAX_RESPONSE_LEN 2048
#define MAX_RECV_LEN 4096

struct connection {
    int fd;
    SSL *ssl;
    enum webserver_status status;
};

struct read_state {
    struct connection conn;
    char req[MAX_RECV_LEN];
    int req_len;
};
void init_read_state(struct read_state *state, struct connection *conn);

struct http_state {
    struct connection conn;
    struct parser_state parser;
    struct db_state db;
};
void init_http_state(struct http_state *state, struct connection *conn);

struct response_state {
    struct connection conn;
    char url[MAX_URL_LEN];
    char resp[MAX_RECV_LEN];
    int resp_len;
};
void init_response_state(struct response_state *state, struct connection *conn);

void init_connection(struct connection *conn, int fd);

int accept_connection(struct connection *conn, int use_ssl);
int read_request(struct read_state *state);
int parse_request(char *req, int req_len, struct http_state *state);
int write_response(struct response_state *state);
int close_connection(struct connection *conn);
int access_database(char *url, struct db_state *state);

int has_regex(char *url);

int craft_error_response(char *url, char *response);
int craft_nonregex_response(char *url, char *response);
int craft_regex_response(char *url, char *response);


#endif
