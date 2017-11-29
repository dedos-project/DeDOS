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
#include "webserver/write_msu.h"
#include "socket_msu.h"
#include "webserver/connection-handler.h"
#include "msu_state.h"
#include "logging.h"
#include "routing_strategies.h"
#include "profiler.h"
#include "local_msu.h"

static int write_http_response(struct local_msu *self,
                               struct msu_msg *msg) {
    struct response_state *resp_in = msg->data;

    size_t size = 0;
    struct response_state *resp = msu_get_state(self, &msg->hdr.key, &size);
    if (resp == NULL) {
        resp = msu_init_state(self, &msg->hdr.key, sizeof(*resp));
        memcpy(resp, resp_in, sizeof(*resp_in));
    }

    int rtn = write_response(resp);
    if (rtn & WS_ERROR) {
        msu_error(self, NULL, 0);
        msu_remove_fd_monitor(resp->conn.fd);
        if (close_connection(&resp->conn) == WS_ERROR) {
            msu_error(self, NULL, 0);
        }
        msu_free_state(self, &msg->hdr.key);
        free(resp_in);
        return -1;
    } else if (rtn & (WS_INCOMPLETE_READ | WS_INCOMPLETE_WRITE)) {
        rtn = msu_monitor_fd(resp->conn.fd, RTN_TO_EVT(rtn), self, &msg->hdr);
        free(resp_in);
        return rtn;
    } else {
        PROFILE_EVENT(msg->hdr, PROF_DEDOS_EXIT);
        msu_remove_fd_monitor(resp->conn.fd);
        if (close_connection(&resp->conn) == WS_ERROR) {
            msu_error(self, NULL, 0);
        }
        log(LOG_WEBSERVER_WRITE, "Successful connection to fd %d closed",
                   resp->conn.fd);
        log(LOG_WEBSERVER_WRITE, "Wrote request: %s", resp->resp);
        msu_free_state(self, &msg->hdr.key);
        free(resp_in);
        return 0;
    }
}

struct msu_type WEBSERVER_WRITE_MSU_TYPE = {
    .name = "Webserver_write_MSU",
    .id = WEBSERVER_WRITE_MSU_TYPE_ID,
    .receive = write_http_response,
    .route = route_to_origin_runtime
};


