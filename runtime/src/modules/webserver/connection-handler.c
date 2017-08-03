#include "modules/webserver/connection-handler.h"
#include "logging.h"
#include "modules/webserver/sslops.h"
#include "modules/webserver/socketops.h"
#include "modules/webserver/dbops.h"
#include "modules/webserver/regex.h"
#include <unistd.h>

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

#ifndef LOG_CONNECTION_INFO
#define LOG_CONNECTION_INFO 0
#endif

void init_connection_state(struct connection_state *state, int fd) {
    state->fd = fd;
    init_request_state(&state->req_state);
    // This will have to be changed if accepting connections is done
    // outside of epollops
    state->conn_status = CON_ACCEPTED;
    state->ssl = NULL;
    state->response[0] = '\0';
    state->db_fd = -1;
}

int accept_connection(struct connection_state *state, int use_ssl) {
    if (!use_ssl) {
       log_error("Not implemented, assuming connection is already accepted");
       return -1;
    }

    if (state->ssl == NULL) {
        state->ssl = init_ssl_connection(state->fd);
    }

    int rtn = accept_ssl(state->ssl);
    if (rtn == 0) {
        state->conn_status = CON_READING;
        log_custom(LOG_CONNECTION_INFO, "Accepted SSL connection on fd %d",
               state->fd);
    } else if (rtn == -1) {
        state->conn_status = CON_ERROR;
        log_error("Error accepting SSL");
        return -1;
    } else {
        state->conn_status = CON_SSL_CONNECTING;
        log_custom(LOG_CONNECTION_INFO, "SSL accept incomplete (fd: %d)", state->fd);
    }
    return 0;
}

int read_connection(struct connection_state *state) {
    int use_ssl = (state->ssl != NULL);

    int bytes_read;
    if (use_ssl) {
        bytes_read = read_ssl(state->ssl, state->buf, MAX_RECV_LEN);
    } else {
        bytes_read = read_socket(state->fd, state->buf, MAX_RECV_LEN);
    }

    if (bytes_read == 0) {
        log_custom(LOG_CONNECTION_INFO, "Read incomplete (fd: %d)", state->fd);
        return 0;
    } else if (bytes_read < 0) {
        state->conn_status = CON_ERROR;
        //log_error("Error reading");
        return -1;
    }

    log_custom(LOG_CONNECTION_INFO, "Completed reading (fd: %d)", state->fd);
    state->buf_len = bytes_read;
    state->conn_status = CON_PARSING;

    return 0;
}

int parse_connection(struct connection_state *state) {
    enum request_status req_status =
        parse_request(&state->req_state, state->buf, state->buf_len);

    switch (req_status) {
        case REQ_COMPLETE:
            log_custom(LOG_CONNECTION_INFO, "Request complete (fd: %d)", state->fd);
            state->conn_status = CON_WRITING;
            return 0;
        case REQ_INCOMPLETE:
            log_custom(LOG_CONNECTION_INFO, "Partial request received (fd: %d)", state->fd);
            state->conn_status = CON_READING;
            return 1;
        case REQ_ERROR:
            log_error("Error parsing request");
            return -1;
        default:
            log_error("Unknown request status: %d", req_status);
            return -1;
    }
}

#define DEFAULT_HTTP_HEADER\
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n" \

#define DEFAULT_HTTP_BODY\
    "<!DOCTYPE html>\n<html>\n<body>\n<h1>Dedos New Runtime</h1>\n</body>\n</html>"

#define REGEX_KEY "regex="
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

int get_connection_resource(struct connection_state *state) {
    char *url = state->req_state.url;
    if (strstr(url, "database") != NULL) {
        int rtn = query_db(state);
        if (rtn < 0) {
            log_error("Error querying database");
        } else if (rtn == 1) {
            return 1;
        }
    }

    return 0;
}

int has_regex(struct connection_state *state) {
    char *url = state->req_state.url;
    char *regex_loc = strstr(url, REGEX_KEY);
    if (regex_loc == NULL) {
        return 0;
    } else {
        return 1;
    }
}

int craft_response(struct connection_state *state) {
    char *url = state->req_state.url;
    char regex_value[MAX_REGEX_VALUE_LEN];
    int rtn = get_regex_value(url, regex_value);

    if (rtn == 0) {
        char html[4096];
        rtn = craft_regex_response(regex_value, html);
        if (rtn < 0) {
            log_error("Error crafting regex response");
            state->conn_status = CON_ERROR;
            return -1;
        }
        sprintf(state->response, DEFAULT_HTTP_HEADER "%s", (int)strlen(html), html);
        state->resp_size = strlen(state->response);
        log_custom(LOG_CONNECTION_INFO, "crafted regex response");
        return 0;
    } else {
        sprintf(state->response, DEFAULT_HTTP_HEADER DEFAULT_HTTP_BODY,
                (int)strlen(DEFAULT_HTTP_BODY));
        state->resp_size = strlen(state->response);
        log_custom(LOG_CONNECTION_INFO, "Crafted normal response");
        return 0;
    }
}

int write_connection(struct connection_state *state) {
    if (state->response[0] == '\0') {
        log_error("Attempted to write connection without a generated response");
    }

    int use_ssl = (state->ssl != NULL);

    int bytes_written;
    if (use_ssl) {
        bytes_written = write_ssl(state->ssl, state->response, state->resp_size);
    } else {
        bytes_written = write_socket(state->fd, state->response, state->resp_size);
    }

    if (bytes_written > 0) {
        log_custom(LOG_CONNECTION_INFO, "wrote response to %d: %s", state->fd, state->response);
        state->conn_status = CON_COMPLETE;
        return 0;
    } else if (bytes_written == 0) {
        log_custom(LOG_CONNECTION_INFO, "wrote partial response (fd=%d)", state->fd);
        return 0;
    } else {
        log_error("Error writing response");
        return -1;
    }
}

int close_connection(struct connection_state *conn) {

    if (conn->ssl != NULL) {
        close_ssl(conn->ssl);
    }

    int rtn = close(conn->fd);
    if (rtn != 0) {
        log_perror("Error closing connection fd=%d", conn->fd);
        return -1;
    }

    log_custom(LOG_CONNECTION_INFO, "Closed fd %d", conn->fd);
    conn->conn_status = CON_CLOSED;

    return 0;
}
