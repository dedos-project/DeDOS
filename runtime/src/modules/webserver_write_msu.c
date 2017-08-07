#include "modules/webserver_write_msu.h"
#include "modules/blocking_socket_handler_msu.h"
#include "modules/webserver/connection-handler.h"
#include "communication.h"
#include "dedos_msu_list.h"
#include "msu_state.h"

#ifndef LOG_WEBSERVER_WRITE
#define LOG_WEBSERVER_WRITE 0
#endif

static int write_http_response(struct generic_msu *self,
                               struct generic_msu_queue_item *queue_item) {
    struct response_state *resp_in = queue_item->buffer;

    size_t size = 0;
    struct response_state *resp = msu_get_state(self, queue_item->id, 0, &size);
    if (resp == NULL) {
        resp = msu_init_state(self, queue_item->id, 0, sizeof(*resp));
        memcpy(resp, resp_in, sizeof(*resp_in));
    }

    int rtn = write_response(resp);
    if (rtn & WS_ERROR) {
        close_connection(&resp->conn);
        msu_free_state(self, queue_item->id, 0);
        return -1;
    } else if (rtn & (WS_INCOMPLETE_READ | WS_INCOMPLETE_WRITE)) {
        monitor_fd(resp->conn.fd, RTN_TO_EVT(rtn), self);
        return 0;
    } else {
        close_connection(&resp->conn);
        log_custom(LOG_WEBSERVER_WRITE, "Successful connection to fd %d closed",
                   resp->conn.fd);
        log_custom(LOG_WEBSERVER_WRITE, "Wrote request: %s", resp->resp);
        msu_free_state(self, queue_item->id, 0);
        return 0;
    }
}

static struct msu_endpoint *default_within_ip_routing(struct msu_type *type,
                                     struct generic_msu *sender,
                                     struct generic_msu_queue_item *queue_item) {
    uint32_t origin_ip = queue_item->path[0].ip_address;
    struct msu_endpoint *dest = round_robin_within_ip(type, sender, origin_ip);
    if (dest == NULL) {
        log_error("can't find appropriate webserver write MSU");
    }
    char ipstr[32];
    ipv4_to_string(ipstr, origin_ip);
    log_custom(LOG_WEBSERVER_WRITE, "Routing write to endpoint %d (ip %s)", dest->id, ipstr);;
    return dest;
}

const struct msu_type WEBSERVER_WRITE_MSU_TYPE = {
    .name = "webserver_write_msu",
    .layer = DEDOS_LAYER_APPLICATION,
    .type_id = DEDOS_WEBSERVER_WRITE_MSU_ID,
    .proto_number = 999, //?
    .init = NULL,
    .destroy = NULL,
    .receive = write_http_response,
    .receive_ctrl = NULL,
    .route = default_within_ip_routing,
    .deserialize = default_deserialize,
    .send_local = default_send_local,
    .send_remote = default_send_remote,
    .generate_id = NULL
};
