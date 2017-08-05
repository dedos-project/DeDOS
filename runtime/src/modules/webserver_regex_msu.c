#include "generic_msu.h"
#include "routing.h"
#include "dedos_msu_list.h"
#include "modules/webserver_regex_msu.h"
#include "modules/webserver/connection-handler.h"

static int craft_ws_regex_response(struct generic_msu *self,
                                   struct generic_msu_queue_item *queue_item) {
    struct response_state *resp = queue_item->buffer;

    if (!has_regex(resp->url)) {
        log_error("Regex MSU received request without regex!");
        return -1;
    }

    resp->resp_len = craft_regex_response(resp->url, resp->resp);
    return DEDOS_WEBSERVER_WRITE_MSU_ID;
}

const struct msu_type WEBSERVER_REGEX_MSU_TYPE = {
    .name = "webserver_regex_msu",
    .layer = DEDOS_LAYER_APPLICATION,
    .type_id = DEDOS_WEBSERVER_REGEX_MSU_ID,
    .proto_number = 999, //?
    .init = NULL,
    .destroy = NULL,
    .receive = craft_ws_regex_response,
    .receive_ctrl = NULL,
    .route = shortest_queue_route,
    .deserialize = default_deserialize,
    .send_local = default_send_local,
    .send_remote = default_send_remote,
    .generate_id = NULL
};

