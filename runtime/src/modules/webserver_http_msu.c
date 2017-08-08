#include "modules/webserver_http_msu.h"
#include "modules/webserver/connection-handler.h"
#include "dedos_msu_list.h"
#include "modules/webserver/dbops.h"
#include "modules/webserver/epollops.h"
#include "modules/blocking_socket_handler_msu.h"
#include "modules/socket_registry_msu.h"
#include "msu_state.h"
#include "msu_queue.h"

#ifndef LOG_HTTP_MSU
#define LOG_HTTP_MSU 0
#endif

#ifndef LOG_PARTIAL_READS
#define LOG_PARTIAL_READS 0
#endif

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

static int handle_db(struct http_state *http_state,
                     struct generic_msu *self,
                     struct generic_msu_queue_item *queue_item) {
    http_state->db.data = self->internal_state;
    int rtn = access_database(http_state->parser.url, &http_state->db);

    if (rtn & WS_ERROR) {
        msu_free_state(self, &queue_item->key, 0);
        // TODO: Send to write?
        log_warn("HALP!");
        return -1;
    } else if (rtn & (WS_INCOMPLETE_READ | WS_INCOMPLETE_WRITE)) {
        log_custom(LOG_HTTP_MSU, "Partial database access, so requeing state %p", http_state);
        http_state->conn.status = CON_DB_REQUEST;
        mask_monitor_fd(http_state->db.db_fd,
                        RTN_TO_EVT(rtn),
                        self,
                        queue_item);
        return 0;
    } else {
        struct response_state *resp = malloc(sizeof(*resp));
        init_response_state(resp, &http_state->conn);
        strcpy(resp->url, http_state->parser.url);
        msu_free_state(self, &queue_item->key, 0);
        queue_item->buffer = resp;
        queue_item->buffer_len = sizeof(*resp);
        if (!has_regex(resp->url)) {
            log_custom(LOG_HTTP_MSU, "Crafting response for url %s", resp->url);
            resp->resp_len = craft_nonregex_response(resp->url, resp->resp);
            return DEDOS_WEBSERVER_WRITE_MSU_ID;
        }
        return DEDOS_WEBSERVER_REGEX_ROUTING_MSU_ID;
    }
}



static int handle_parsing(struct read_state *read_state,
                          struct http_state *http_state,
                          struct generic_msu *self,
                          struct generic_msu_queue_item *queue_item){
    if (read_state->req_len == -1) {
        log_custom(LOG_HTTP_MSU, "Clearing state (fd: %d)", read_state->conn.fd);
        msu_free_state(self, &queue_item->key, 0);
        return 0;
    }
    log_custom(LOG_HTTP_MSU, "Parsing request: %s (fd: %d)", read_state->req, read_state->conn.fd);
    int rtn = parse_request(read_state->req, read_state->req_len, http_state);

    if (rtn & WS_COMPLETE) {
        return handle_db(http_state, self, queue_item);
    } else if (rtn & WS_ERROR) {
        msu_free_state(self, &queue_item->key, 0);
        return -1;
    } else {
        http_state->conn.status = CON_READING;
        log_custom(LOG_PARTIAL_READS, "got partial request %s (fd: %d) ",
                   read_state->req, read_state->conn.fd);
        return DEDOS_WEBSERVER_READ_MSU_ID;
    }

}

static int craft_http_response(struct generic_msu *self,
                               struct generic_msu_queue_item *queue_item) {

    struct read_state *read_state = queue_item->buffer;

    size_t size = 0;
    struct http_state *http_state = msu_get_state(self, &queue_item->key, 0, &size);
    if (http_state == NULL) {
        http_state = msu_init_state(self, &queue_item->key, 0, sizeof(*read_state));
        read_state->conn.status = CON_READING;
        init_http_state(http_state, &read_state->conn);
        memcpy(&http_state->conn, &read_state->conn, sizeof(read_state->conn));
    } else {
        log_custom(LOG_HTTP_MSU, "Retrieved state: %p (status: %d)",
                   http_state, http_state->conn.status);
        if (http_state->conn.status != CON_DB_REQUEST) {
            log_custom(LOG_PARTIAL_READS, "Recovering partial read state ID: %u", 
                       queue_item->key.id);
        }
    }
    int rtn;
    switch (http_state->conn.status) {
        case NIL:
        case CON_READING:
            log_custom(LOG_HTTP_MSU, "got CON_READING");
            rtn = handle_parsing(read_state, http_state, self, queue_item);
            if (rtn < 0) {
                log_error("Error processing fd %d, ID %u", read_state->conn.fd, 
                          (queue_item->key.id));
            } else if (rtn == DEDOS_WEBSERVER_READ_MSU_ID) {
                log_custom(LOG_PARTIAL_READS, "Partial request ID: %u",
                           (queue_item->key.id));
            }
            return rtn;
        case CON_DB_REQUEST:
            log_custom(LOG_HTTP_MSU, "got CON_DB_REQUEST");
            return handle_db(http_state, self, queue_item);
        default:
            log_error("Received unknown packet status: %d", read_state->conn.status);
            return -1;
    }
}

static int http_init(struct generic_msu *self, struct create_msu_thread_data UNUSED *data) {
    void *db_memory = allocate_db_memory();
    self->internal_state = db_memory;
    //self->internal_state = malloc(100);
    return 0;
}

static void http_destroy(struct generic_msu *self) {
    free(self->internal_state);
}

const struct msu_type WEBSERVER_HTTP_MSU_TYPE = {
    .name = "webserver_http_msu",
    .layer = DEDOS_LAYER_APPLICATION,
    .type_id = DEDOS_WEBSERVER_HTTP_MSU_ID,
    .proto_number = 999, //?
    .init = http_init,
    .destroy = http_destroy,
    .receive = craft_http_response,
    .receive_ctrl = NULL,
    .route = default_routing,
    .deserialize = default_deserialize,
    .send_local = default_send_local,
    .send_remote = default_send_remote,
    .generate_id = NULL
};

