/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

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
#ifndef REQUEST_PARSER_H_
#define REQUEST_PARSER_H_
#include "webserver/http_parser.h"

#define MAX_URL_LEN 256

struct parser_state {
    char url[MAX_URL_LEN];
    int url_len;
    int headers_complete;
    ssize_t (*recv_fn)(int fd, SSL *ssl, char *buf);
    http_parser_settings settings;
    http_parser parser;
};

void init_parser_state(struct parser_state *state);

enum parser_status {
    REQ_INCOMPLETE,
    REQ_COMPLETE,
    REQ_ERROR
};

int parse_http(struct parser_state *state, char *buf, ssize_t bytes);

#endif
