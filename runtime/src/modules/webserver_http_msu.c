#include "modules/webserver_http_msu.h"
#include "modules/webserver_routing_msu.h"
#include "modules/webserver/connection-handler.h"
#include "dedos_msu_list.h"
#include "modules/webserver/dbops.h"
#include "modules/webserver/epollops.h"
#include "modules/blocking_socket_handler_msu.h"

#ifndef LOG_HTTP_MSU
#define LOG_HTTP_MSU 0
#endif

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

static int craft_http_response(struct generic_msu *self,
                               struct generic_msu_queue_item *queue_item) {
    if (queue_item->buffer == NULL) {
        log_error("Cannot craft HTTP response from NULL queue item");
        return -1;
    }

    struct webserver_state *ws_state = queue_item->buffer;
    struct connection_state *state = &ws_state->conn_state;

    int rtn;
    switch (state->conn_status) {
        case CON_PARSING:
            rtn = parse_connection(state);
            if (rtn == -1) {
                log_error("Error parsing HTTP!");
                return -1;
            }
            if (rtn == 1) {
                log_custom(LOG_HTTP_MSU, "HTTP parsing incomplete");
                return DEDOS_WEBSERVER_ROUTING_MSU_ID;
            }
        case CON_DB_CONNECTING:
        case CON_DB_SEND:
        case CON_DB_RECV:
            state->data = self->internal_state;
            rtn = get_connection_resource(state);
            if (rtn == 1) {
                log_custom(LOG_HTTP_MSU, "Database connection incomplete");
                if (state->conn_status == CON_DB_SEND || state->conn_status == CON_DB_CONNECTING) {
                    add_proxy_fd(state->db_fd, EPOLLOUT, state->fd);
                } else if (state->conn_status == CON_DB_RECV) {
                    add_proxy_fd(state->db_fd, EPOLLIN, state->fd);
                }
                return 0;
            }
            if (rtn != 0) {
                log_error("Error getting requested resource");
                return -1;
            }

            int needs_regex = has_regex(state);
            if (needs_regex) {
                state->conn_status = CON_PARSING;
                return DEDOS_WEBSERVER_REGEX_ROUTING_MSU_ID;
            }

            state->conn_status = CON_WRITING;
            rtn = craft_response(state);
            if (rtn == -1) {
                log_error("Error crafting HTTP response");
                return -1;
            }
            return DEDOS_WEBSERVER_ROUTING_MSU_ID;
        default:
            log_error("MSU %d received queue item with improper status: %d",
                      self->id, (int)state->conn_status);
            return -1;
    }
}

static int http_init(struct generic_msu *self, struct create_msu_thread_data UNUSED *data) {
    void *db_memory = allocate_db_memory();
    self->internal_state = db_memory;
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

