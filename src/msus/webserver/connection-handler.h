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

int craft_nonregex_response(char *url, char *response);
int craft_regex_response(char *url, char *response);


#endif
