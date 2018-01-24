/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

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
#include "dedos.h"
#include "local_msu.h"
#include "msu_type.h"
#include "msu_message.h"
#include "msu_calls.h"
#include "logging.h"
#include "routing_strategies.h"

#include "webserver/write_msu.h"
#include "webserver/regex_msu.h"
#include "webserver/connection-handler.h"
#include "webserver/httpops.h"

static int craft_ws_regex_response(struct local_msu *self,
                                   struct msu_msg *msg) {
    struct response_state *resp = msg->data;

    if (!has_regex(resp->url)) {
        log_error("Regex MSU received request without regex");
        return -1;
    }

    resp->body_len = craft_regex_response(resp->url, resp->body);
    resp->header_len = generate_header(resp->header, 200, MAX_HEADER_LEN,
                                       resp->body_len, "text/html");
    call_msu_type(self, &WEBSERVER_WRITE_MSU_TYPE, &msg->hdr, sizeof(*resp), resp);
    return 0;
}

struct msu_type WEBSERVER_REGEX_MSU_TYPE = {
    .name = "Webserver_regex_msu",
    .id = WEBSERVER_REGEX_MSU_TYPE_ID,
    .receive = craft_ws_regex_response,
    .route = shortest_queue_route
};
