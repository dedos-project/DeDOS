#include "webserver/write_msu.h"
#include "socket_msu.h"
#include "webserver/connection-handler.h"
#include "msu_state.h"
#include "logging.h"
#include "routing_strategies.h"
#include "profiler.h"

static int write_http_response(struct local_msu *self,
                               struct msu_msg *msg) {
    struct response_state *resp_in = msg->data;

    size_t size = 0;
    struct response_state *resp = msu_get_state(self, &msg->hdr.key, &size);
    if (resp == NULL) {
        resp = msu_init_state(self, &msg->hdr.key, sizeof(*resp));
        memcpy(resp, resp_in, sizeof(*resp_in));
    }

    if (resp->conn.status == CON_ERROR) {
        msu_remove_fd_monitor(resp->conn.fd);
        close_connection(&resp->conn);
        msu_free_state(self, &msg->hdr.key);
        free(resp_in);
        return 0;
    }

    int rtn = write_response(resp);
    if (rtn & WS_ERROR) {
        msu_remove_fd_monitor(resp->conn.fd);
        close_connection(&resp->conn);
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
        close_connection(&resp->conn);
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


