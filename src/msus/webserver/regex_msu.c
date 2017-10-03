#include "local_msu.h"
#include "msu_type.h"
#include "msu_message.h"
#include "msu_calls.h"
#include "logging.h"
#include "routing_strategies.h"

#include "webserver/write_msu.h"
#include "webserver/regex_msu.h"
#include "webserver/connection-handler.h"

static int craft_ws_regex_response(struct local_msu *self,
                                   struct msu_msg *msg) {
    struct response_state *resp = msg->data;

    if (!has_regex(resp->url)) {
        log_error("Regex MSU received request without regex");
        return -1;
    }

    resp->resp_len = craft_regex_response(resp->url, resp->resp);
    call_msu_type(self, &WEBSERVER_WRITE_MSU_TYPE, &msg->hdr, sizeof(*resp), resp);
    return 0;
}

struct msu_type WEBSERVER_REGEX_MSU_TYPE = {
    .name = "Webserver_regex_msu",
    .id = WEBSERVER_REGEX_MSU_TYPE_ID,
    .receive = craft_ws_regex_response,
    .route = shortest_queue_route
};
