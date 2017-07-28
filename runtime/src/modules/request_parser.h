#ifndef REQUEST_PARSER_H_
#define REQUEST_PARSER_H_
#include "http-parser/http_parser.h"

struct request_state {
    char url[256];
    int url_len;
    int headers_complete;
    ssize_t (*recv_fn)(int fd, SSL *ssl, char *buf);
    http_parser_settings settings;
    http_parser parser;
};

void init_request_state(struct request_state *state);

enum request_status {
    REQ_INCOMPLETE,
    REQ_COMPLETE,
    REQ_ERROR
};

enum request_status parse_request(struct request_state *state, char *buf, ssize_t bytes);

#endif
