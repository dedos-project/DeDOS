#include "webserver/http_msu.h"
#include "webserver/connection-handler.h"
#include "webserver/dbops.h"
#include "webserver/write_msu.h"
#include "webserver/read_msu.h"
#include "socket_msu.h"
#include "logging.h"
#include "epollops.h"
#include "msu_state.h"
#include "unused_def.h"
#include "local_msu.h"
#include "webserver/regex_msu.h"


static int handle_db(struct http_state *http_state,
                     struct local_msu *self,
                     struct msu_msg *msg) {

    http_state->db.data = self->msu_state;
    int rtn = access_database(http_state->parser.url, &http_state->db);

    if (rtn & WS_ERROR) {
        msu_free_state(self, &msg->hdr->key);
        log_warn("HELP!");
        return -1;
    } else if (rtn & (WS_INCOMPLETE_READ | WS_INCOMPLETE_WRITE)) {
        log(LOG_HTTP_MSU, "Partial db access, requeuing state %p", http_state);
        http_state->conn.status = CON_DB_REQUEST;
        return msu_monitor_fd(http_state->db.db_fd, RTN_TO_EVT(rtn), self, msg->hdr);
    } else {
        struct response_state *resp = malloc(sizeof(*resp));
        init_response_state(resp, &http_state->conn);
        strcpy(resp->url, http_state->parser.url);
        msu_free_state(self, &msg->hdr->key);
        // SHOULDI: be freeing this data?
        free(msg->data);

        if (!has_regex(resp->url)) {
            log(LOG_HTTP_MSU, "Crafting response for url %s", resp->url);
            resp->resp_len = craft_nonregex_response(resp->url, resp->resp);
            return call_msu(self, &WEBSERVER_WRITE_MSU_TYPE, msg->hdr, sizeof(*resp), resp);
        }

        return call_msu(self, &WEBSERVER_REGEX_MSU_TYPE, msg->hdr, sizeof(*resp), resp);
    }
}

static int handle_parsing(struct read_state *read_state,
                          struct http_state *http_state,
                          struct local_msu *self,
                          struct msu_msg *msg) {
    if (read_state->req_len == -1) {
        log(LOG_HTTP_MSU, "Clearing state (fd: %d)", read_state->conn.fd);
        msu_free_state(self, &msg->hdr->key);
        return 0;
    }
    log(LOG_HTTP_MSU, "Parsing request: %s (fd: %d)", read_state->req, read_state->conn.fd);
    int rtn = parse_request(read_state->req, read_state->req_len, http_state);

    if (rtn & WS_COMPLETE) {
        return handle_db(http_state, self, msg);
    } else if (rtn & WS_ERROR) {
        msu_free_state(self, &msg->hdr->key);
        return -1;
    } else {
        http_state->conn.status = CON_READING;
        log(LOG_PARTIAL_READS, "Got partial request %s (fd: %d)",
                   read_state->req, read_state->conn.fd);
        return call_msu(self, &WEBSERVER_READ_MSU_TYPE, msg->hdr, msg->data_size, msg->data);
    }
}

static int craft_http_response(struct local_msu *self,
                               struct msu_msg *msg) {
    struct read_state *read_state = msg->data;

    size_t size = 0;
    struct http_state *http_state = msu_get_state(self, &msg->hdr->key, &size);
    if (http_state == NULL) {
        http_state = msu_init_state(self, &msg->hdr->key, sizeof(*http_state));
        read_state->conn.status = CON_READING;
        init_http_state(http_state, &read_state->conn);
        memcpy(&http_state->conn, &read_state->conn, sizeof(read_state->conn));
    } else {
        log(LOG_HTTP_MSU, "Retrieved state %p (status %d)",
                   http_state, http_state->conn.status);
        if (http_state->conn.status != CON_DB_REQUEST) {
            log(LOG_PARTIAL_READS, "Recovering partial read state ID: %d",
                       msg->hdr->key.id);
        }
    }
    int rtn;
    switch (http_state->conn.status) {
        case NIL:
        case CON_READING:
            log(LOG_HTTP_MSU, "got CON_READING");
            rtn = handle_parsing(read_state, http_state, self, msg);
            if (rtn < 0) {
                log_error("Error processing fd %d, ID %u", read_state->conn.fd, 
                          (msg->hdr->key.id));
            }
            return rtn;
        case CON_DB_REQUEST:
            log(LOG_HTTP_MSU, "got CON_DB_REQUEST");
            return handle_db(http_state, self, msg);
        default:
            log_error("Received unknown packet status: %d", read_state->conn.status);
            return -1;
    }
}

static int http_init(struct local_msu *self, struct msu_init_data UNUSED *data) {
    self->msu_state = allocate_db_memory();
    return 0;
}

static void http_destroy(struct local_msu *self) {
    free(self->msu_state);
}

struct msu_type WEBSERVER_HTTP_MSU_TYPE = {
    .name = "Webserver_HTTP_MSU",
    .id = WEBSERVER_HTTP_MSU_TYPE_ID,
    .init = http_init,
    .destroy = http_destroy,
    .receive = craft_http_response,
};
