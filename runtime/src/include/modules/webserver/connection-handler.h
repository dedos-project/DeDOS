#ifndef CONNECTION_HANDLER_H_
#define CONNECTION_HANDLER_H_

#include <openssl/ssl.h>
#include "request_parser.h"

#define MAX_RESPONSE_LEN 2048
#define MAX_RECV_LEN 4096

enum connection_status {
    NIL,
    CON_ERROR,
    NO_CONNECTION,
    CON_ACCEPTED,
    CON_SSL_CONNECTING,
    CON_READING,
    CON_PARSING,
    CON_DB_CONNECTING,
    CON_DB_SEND,
    CON_DB_RECV,
    CON_WRITING,
    CON_COMPLETE,
    CON_CLOSED,
};

struct connection_state {
    int fd;
    SSL *ssl;
    int db_fd;
    char db_req[16];
    void *data;
    char response[MAX_RESPONSE_LEN];
    int resp_size;
    char buf[MAX_RECV_LEN];
    int buf_len;
    enum connection_status conn_status;
    struct request_state req_state;
};

void init_connection_state(struct connection_state *state, int fd);
int has_regex(struct connection_state *state);

int accept_connection(struct connection_state *state, int use_ssl);
int read_connection(struct connection_state *state);
int parse_connection(struct connection_state *state);
int get_connection_resource(struct connection_state *state);
int craft_response(struct connection_state *state);
int write_connection(struct connection_state *state);
int close_connection(struct connection_state *state);

#endif
