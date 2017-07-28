#include "generic_msu.h"
#include "routing.h"
#include "dedos_msu_list.h"
#include "modules/webserver_routing_msu.h"
#include "modules/webserver_regex_msu.h"
#include "modules/webserver/connection-handler.h"

static int craft_ws_regex_response(struct generic_msu *self,
                                   struct generic_msu_queue_item *queue_item) {
    if (queue_item->buffer == NULL) {
        log_error("Cannot craft REGEX response from NULL queue item");
        return -1;
    }

    struct webserver_state *ws_state = queue_item->buffer;
    struct connection_state *state = &ws_state->conn_state;

    int rtn;
    switch (state->conn_status) {
        case CON_PARSING:
            if (!has_regex(state)) {
                log_error("REGEX msu %d got non-regex HTTP", self->id);
                return -1;
            }
            rtn = craft_response(state);
            if (rtn == -1) {
                log_error("Error crafting HTTP response");
                return -1;
            }
            return DEDOS_WEBSERVER_WRITE_MSU_ID;
        default:
            log_error("MSU %d received queue item with improper status: %d",
                     self->id, state->conn_status);
            return -1;
    }
}

const struct msu_type WEBSERVER_REGEX_MSU_TYPE = {
    .name = "webserver_http_msu",
    .layer = DEDOS_LAYER_APPLICATION,
    .type_id = DEDOS_WEBSERVER_REGEX_MSU_ID,
    .proto_number = 999, //?
    .init = NULL,
    .destroy = NULL,
    .receive = craft_ws_regex_response,
    .receive_ctrl = NULL,
    .route = default_routing,
    .deserialize = default_deserialize,
    .send_local = default_send_local,
    .send_remote = default_send_remote,
    .generate_id = NULL
};

