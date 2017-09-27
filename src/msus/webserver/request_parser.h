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
