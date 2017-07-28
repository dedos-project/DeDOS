#include "modules/webserver_http_msu.h"
#include "modules/webserver_routing_msu.h"
#include "modules/webserver/connection-handler.h"
#include "dedos_msu_list.h"

#ifndef LOG_HTTP_MSU
#define LOG_HTTP_MSU 0
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
            rtn = get_connection_resource(state);
            if (rtn != 0) {
                log_error("Error getting requested resource");
                return -1;
            }
            int needs_regex = has_regex(state);
            if (needs_regex) {
                return DEDOS_WEBSERVER_REGEX_MSU_ID;
            }
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

const struct msu_type WEBSERVER_HTTP_MSU_TYPE = {
    .name = "webserver_http_msu",
    .layer = DEDOS_LAYER_APPLICATION,
    .type_id = DEDOS_WEBSERVER_HTTP_MSU_ID,
    .proto_number = 999, //?
    .init = NULL,
    .destroy = NULL,
    .receive = craft_http_response,
    .receive_ctrl = NULL,
    .route = default_routing,
    .deserialize = default_deserialize,
    .send_local = default_send_local,
    .send_remote = default_send_remote,
    .generate_id = NULL
};

