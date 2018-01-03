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
#include "webserver/http_msu.h"
#include "webserver/connection-handler.h"
#include "webserver/dbops.h"
#include "webserver/cache_msu.h"
#include "webserver/write_msu.h"
#include "webserver/read_msu.h"
#include "socket_msu.h"
#include "msu_calls.h"
#include "logging.h"
#include "epollops.h"
#include "msu_state.h"
#include "unused_def.h"
#include "local_msu.h"
#include "webserver/regex_routing_msu.h"

static int send_error(struct local_msu *self, struct http_state *http_state,
                      struct msu_msg_hdr *hdr) {
    struct response_state *resp = malloc(sizeof(*resp));
    init_response_state(resp, &http_state->conn);
    resp->body_len = craft_error_response(resp->url, resp->body);
    return call_msu_type(self, &WEBSERVER_WRITE_MSU_TYPE, hdr, sizeof(*resp), resp);
}

static int handle_db(struct http_state *http_state,
                     struct local_msu *self,
                     struct msu_msg *msg) {

    http_state->db.data = self->msu_state;
    int rtn = access_database(http_state->parser.url, &http_state->db);

    if (rtn & WS_ERROR) {
        log_warn("Could not access database!");
        msu_error(self, &msg->hdr, 0);

        send_error(self, http_state, &msg->hdr);
        msu_free_state(self, &msg->hdr.key);
        return -1;
    } else if (rtn & (WS_INCOMPLETE_READ | WS_INCOMPLETE_WRITE)) {
        log(LOG_HTTP_MSU, "Partial db access, requeuing state %p", http_state);
        http_state->conn.status = CON_DB_REQUEST;
        return msu_monitor_fd(http_state->db.db_fd, RTN_TO_EVT(rtn), self, &msg->hdr);

    } else if (rtn & WS_COMPLETE) {
        struct response_state *resp = malloc(sizeof(*resp));
        init_response_state(resp, &http_state->conn);
        strcpy(resp->url, http_state->parser.url);
        msu_free_state(self, &msg->hdr.key);

        if (!has_regex(resp->url)) {
            log(LOG_HTTP_MSU, "Crafting response for url %s", resp->url);
            return call_msu_type(self, &WEBSERVER_CACHE_MSU_TYPE, &msg->hdr, sizeof(*resp), resp);
        }

        return call_msu_type(self, &WEBSERVER_REGEX_ROUTING_MSU_TYPE, &msg->hdr, sizeof(*resp), resp);
    }
    log_error("Unknown return code from database access: %d", rtn);
    msu_error(self, &msg->hdr, 0);
    return -1;
}

static int clear_state(struct local_msu *self, struct msu_msg *msg) {
    log(LOG_HTTP_MSU, "Clearing state due to received error message");
    msu_free_state(self, &msg->hdr.key);
    return 0;
}

static int handle_parsing(struct read_state *read_state,
                          struct http_state *http_state,
                          struct local_msu *self,
                          struct msu_msg *msg) {
    if (read_state->req_len == -1) {
        log(LOG_HTTP_MSU, "Clearing state (fd: %d)", read_state->conn.fd);
        msu_free_state(self, &msg->hdr.key);
        return 0;
    }
    log(LOG_HTTP_MSU, "Parsing request: %s (fd: %d)", read_state->req, read_state->conn.fd);
    int rtn = parse_request(read_state->req, read_state->req_len, http_state);

    if (rtn & WS_COMPLETE) {
        free(read_state);
        return handle_db(http_state, self, msg);
    } else if (rtn & WS_ERROR) {
        send_error(self, http_state, &msg->hdr);
        msu_free_state(self, &msg->hdr.key);
        return 0;
    } else {
        http_state->conn.status = CON_READING;
        log(LOG_PARTIAL_READS, "Got partial request %.*s (fd: %d)",
                   read_state->req_len, read_state->req, read_state->conn.fd);
        return call_msu_type(self, &WEBSERVER_READ_MSU_TYPE, &msg->hdr, msg->data_size, msg->data);
    }
}

static int craft_http_response(struct local_msu *self,
                               struct msu_msg *msg) {
    struct read_state *read_state = msg->data;

    size_t size = 0;
    struct http_state *http_state = msu_get_state(self, &msg->hdr.key, &size);
    int retrieved = 0;
    if (http_state == NULL) {
        http_state = msu_init_state(self, &msg->hdr.key, sizeof(*http_state));
        read_state->conn.status = CON_READING;
        init_http_state(http_state, &read_state->conn);
        memcpy(&http_state->conn, &read_state->conn, sizeof(read_state->conn));
    } else {

        log(LOG_HTTP_MSU, "Retrieved state %p (status %d)",
                   http_state, http_state->conn.status);
        retrieved = 1;
        if (http_state->conn.status != CON_DB_REQUEST) {
            log(LOG_PARTIAL_READS, "Recovering partial read state ID: %u",
                       msg->hdr.key.id);
        }
    }
    if (msg->data_size == sizeof(*read_state) && read_state->req_len == -1) {
        if (http_state->conn.status == CON_DB_REQUEST) {
            http_state->conn.status = NO_CONNECTION;
            return 0;
        } else {
            msu_free_state(self, &msg->hdr.key);
            return 0;
        }
    }


    int rtn;
    switch (http_state->conn.status) {
        case NIL:
        case CON_READING:
            log(LOG_HTTP_MSU, "got CON_READING");
            rtn = handle_parsing(read_state, http_state, self, msg);
            if (rtn < 0) {
                log_error("Error processing fd %d, ID %u, retrieved %d", read_state->conn.fd,
                          (msg->hdr.key.id), retrieved);
            }
            return rtn;
        case CON_DB_REQUEST:
            log(LOG_HTTP_MSU, "got CON_DB_REQUEST");
            free(read_state);
            return handle_db(http_state, self, msg);
        case NO_CONNECTION:
            msu_free_state(self, &msg->hdr.key);
            return 0;
        default:
            log_error("Received unknown packet status: %d", read_state->conn.status);
            return -1;
    }
}

static int http_init(struct local_msu *self, struct msu_init_data *data) {
    self->msu_state = allocate_db_memory();

    char *init_data = data->init_data;

    char *ip, *saveptr;

    if ((ip = strtok_r(init_data, " ", &saveptr)) == NULL) {
        log_error("Did not provide IP for DB initialization to http MSU");
        return -1;
    }
    char *port_str;
    if ((port_str = strtok_r(NULL, " ", &saveptr)) == NULL) {
        log_error("Did not provide port for DB initialization to http MSU");
        return -1;
    }
    int port = atoi(port_str);
    char *n_files_str;
    if ((n_files_str = strtok_r(NULL, " ", &saveptr)) == NULL) {
        log_error("Did not provide n_files for DB initialization to http MSU");
        return -1;
    }
    int n_files = atoi(n_files_str);

    init_db(ip, port, n_files);
    return 0;
}

static void http_destroy(struct local_msu *self) {
    free_db_memory(self->msu_state);
}

struct msu_type WEBSERVER_HTTP_MSU_TYPE = {
    .name = "Webserver_HTTP_MSU",
    .id = WEBSERVER_HTTP_MSU_TYPE_ID,
    .init = http_init,
    .destroy = http_destroy,
    .receive = craft_http_response,
    .receive_error = clear_state
};
