#include "webserver/connection-handler.h"
#include "logging.h"
#include "webserver/sslops.h"
#include "webserver/socketops.h"
#include "webserver/dbops.h"
#include "webserver/regex.h"
#include "webserver/request_parser.h"
#include <unistd.h>

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

void init_connection(struct connection *conn, int fd) {
    conn->fd = fd;
    conn->ssl = NULL;
    conn->status = CON_ACCEPTED;
}

void init_read_state(struct read_state *state, struct connection *conn) {
    state->conn = *conn;
    state->req[0] = '\0';
    state->req_len = 0;
}

void init_http_state(struct http_state *state, struct connection *conn) {
    state->conn = *conn;
    init_parser_state(&state->parser);
    bzero(&state->db, sizeof(state->db));
}

void init_response_state(struct response_state *state, struct connection *conn) {
    state->conn = *conn;
    state->url[0] = '\0';
    state->resp[0] = '\0';
    state->resp_len = 0;
}

int accept_connection(struct connection *conn, int use_ssl) {
    if (!use_ssl) {
       log(LOG_CONNECTION_INFO,"Not implemented, assuming connection is already accepted");
       return WS_COMPLETE;
    }

    if (conn->ssl == NULL) {
        conn->ssl = init_ssl_connection(conn->fd);
    }

    int rtn = accept_ssl(conn->ssl);
    if (rtn == WS_COMPLETE) {
        log(LOG_CONNECTION_INFO, "Accepted SSL connection on fd %d", conn->fd);
        return WS_COMPLETE;
    } else if (rtn == -1) {
        //log_error("Error accepting SSL (fd: %d)", conn->fd);
        return WS_ERROR;
    } else {
        log(LOG_CONNECTION_INFO, "SSL accept incomplete (fd: %d)", conn->fd);
        return rtn;
    }
}

int read_request(struct read_state *state) {
    int use_ssl = (state->conn.ssl != NULL);

    int rtn;
    int bytes = MAX_RECV_LEN;
    if (use_ssl) {
        rtn = read_ssl(state->conn.ssl, state->req, &bytes);
    } else {
        rtn = read_socket(state->conn.fd, state->req, &bytes);
    }

    switch (rtn) {
        case WS_INCOMPLETE_READ:
        case WS_INCOMPLETE_WRITE:
            log(LOG_CONNECTION_INFO, "Read incomplete (fd: %d)", state->conn.fd);
            return rtn;
        case WS_COMPLETE:
            state->req_len = bytes;
            log(LOG_CONNECTION_INFO, "Completed reading %d bytes (fd: %d)",
                       bytes, state->conn.fd);
            return WS_COMPLETE;
        case WS_ERROR:
            log(LOG_WS_ERRORS, "Error reading (fd: %d)", state->conn.fd);
            return WS_ERROR;
        default:
            log_error("Unknown return code: %d (fd: %d)", rtn, state->conn.fd);
            return WS_ERROR;
    }
}

int parse_request(char *req, int req_len, struct http_state *state) {
    int status = parse_http(&state->parser, req, req_len);

    switch (status) {
        case WS_COMPLETE:
            log(LOG_CONNECTION_INFO, "Request complete (fd: %d)", state->conn.fd);
            return WS_COMPLETE;
        case WS_INCOMPLETE_READ:
            log(LOG_CONNECTION_INFO, "Partial request received (fd: %d)", state->conn.fd);
            return WS_INCOMPLETE_READ;
        case WS_ERROR:
            log_error("Error parsing request (fd: %d)", state->conn.fd);
            return WS_ERROR;
        default:
            log_error("Unknown status %d returned from parrser (fd: %d)", status, state->conn.fd);
            return WS_ERROR;
    }
}

#define REGEX_KEY "regex="

int has_regex(char *url) {
    char *regex_loc = strstr(url, REGEX_KEY);
    if (regex_loc == NULL) {
        log(LOG_CONNECTION_INFO, "Found no %s in %s", REGEX_KEY, url);
        return 0;
    } else {
        log(LOG_CONNECTION_INFO, "Found regex");
        return 1;
    }
}
#define MAX_REGEX_VALUE_LEN 64

static int get_regex_value(char *url, char *regex) {
    char *regex_start = strstr(url, REGEX_KEY);
    if (regex_start == NULL)
        return -1;
    regex_start += strlen(REGEX_KEY);
    int start_i = (regex_start - url);
    for (int i=start_i; (i-start_i)<MAX_REGEX_VALUE_LEN; i++) {
        switch (url[i]) {
            case '?':
            case '&':
            case '\0':
            case ' ':
                strncpy(regex, &url[start_i], i - start_i);
                regex[i-start_i] = '\0';
                return 0;
            default:
                continue;
        }
    }
    log_error("Requested regex value (%s) too long", regex_start);
    return -1;
}

int access_database(char *url, struct db_state *state) {
    if ( strstr(url, "database") == NULL ) {
        return WS_COMPLETE;
    }
    int rtn = query_db(state);
    switch (rtn) {
        case WS_COMPLETE:
            log(LOG_CONNECTION_INFO, "Successfully queried db");
            return WS_COMPLETE;
        case WS_INCOMPLETE_READ:
        case WS_INCOMPLETE_WRITE:
            log(LOG_CONNECTION_INFO, "Partial DB query");
            return rtn;
        case WS_ERROR:
            log_error("Error querying database");
            return WS_ERROR;
        default:
            log_error("Unknown return %d from database query", rtn);
            return WS_ERROR;
    }
}

#define DEFAULT_HTTP_HEADER\
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n" \

#define DEFAULT_HTTP_BODY\
    "<!DOCTYPE html>\n<html>\n<body>\n<h1>Dedos New Runtime</h1>\n</body>\n</html>"


int craft_nonregex_response(char UNUSED *url, char *response) {
    int n = sprintf(response, DEFAULT_HTTP_HEADER DEFAULT_HTTP_BODY,
            (int)strlen(DEFAULT_HTTP_BODY));
    log(LOG_CONNECTION_INFO, "Crafted non-regex response");
    return n;
}

int craft_regex_response(char *url, char *response) {
    char regex_value[MAX_REGEX_VALUE_LEN];
    int rtn = get_regex_value(url, regex_value);
    if (rtn != 0) {
        log_error("Non-regex URL passed to craft_regex_response!");
        return -1;
    } else {
        char html[4096];
        rtn = regex_html(regex_value, html);
        if (rtn < 0) {
            log_error("Error crafting regex response");
            return -1;
        }
        sprintf(response, DEFAULT_HTTP_HEADER "%s", (int)strlen(html), html);
        log(LOG_CONNECTION_INFO, "Crafted regex response");
        return strlen(response);
    }
}

int write_response(struct response_state *state) {
    if (state->resp[0] == '\0') {
        log_error("Attempted to write empty respnose");
        return WS_ERROR;
    }

    int use_ssl = (state->conn.ssl != NULL);
    int bytes = state->resp_len;
    int rtn;
    if (use_ssl) {
        rtn = write_ssl(state->conn.ssl, state->resp, &bytes);
    } else {
        rtn = write_socket(state->conn.fd, state->resp, &bytes);
    }

    switch (rtn) {
        case WS_INCOMPLETE_READ:
        case WS_INCOMPLETE_WRITE:
            log(LOG_CONNECTION_INFO, "Write incomplete (fd: %d)", state->conn.fd);
            return rtn;
        case WS_COMPLETE:
            log(LOG_CONNECTION_INFO, "Completed writing response (fd: %d)", state->conn.fd);
            return WS_COMPLETE;
        case WS_ERROR:
            log_error("Error writing response (fd: %d)", state->conn.fd);
            return WS_ERROR;
        default:
            log_error("Unknown return %d from call to write (fd: %d)", rtn, state->conn.fd);
            return WS_ERROR;
    }
}

int close_connection(struct connection *conn) {
    log(LOG_CONNECTION_INFO, "Closing connection (fd: %d)", conn->fd);
    if (conn->ssl != NULL) {
        close_ssl(conn->ssl);
    }

    int rtn = close(conn->fd);
    if (rtn != 0) {
        log_perror("Error closing connection (fd: %d)", conn->fd);
        return WS_ERROR;
    }
    log(LOG_CONNECTION_INFO, "Closed connection (fd: %d)", conn->fd);
    return WS_COMPLETE;
}
