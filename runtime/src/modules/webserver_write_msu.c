#include "modules/webserver_write_msu.h"
#include "modules/blocking_socket_handler_msu.h"
#include "modules/webserver/connection-handler.h"
#include "modules/webserver_routing_msu.h"
#include "dedos_msu_list.h"

static int write_http_response(struct generic_msu *self,
                               struct generic_msu_queue_item *queue_item) {
    if (queue_item->buffer == NULL) {
        log_error("Cannot write HTTP response from NULL queue item");
        return -1;
    }

    struct webserver_state *ws_state = queue_item->buffer;
    struct connection_state *state = &ws_state->conn_state;

    int rtn;
    switch (state->conn_status) {
        case CON_WRITING:
            rtn = write_connection(state);
            if (rtn == -1) {
                return -1;
            }
            if (state->conn_status == CON_COMPLETE) {
                close_connection(state);
                return DEDOS_WEBSERVER_ROUTING_MSU_ID;
            } else {
                monitor_fd(ws_state->fd, EPOLLOUT);
                return 0;
            }
        default:
            log_error("MSU %d received queue item with improper status: %d", 
                      self->id, state->conn_status);
            return -1;
    }
}


const struct msu_type WEBSERVER_WRITE_MSU_TYPE = {
    .name = "webserver_http_msu",
    .layer = DEDOS_LAYER_APPLICATION,
    .type_id = DEDOS_WEBSERVER_WRITE_MSU_ID,
    .proto_number = 999, //?
    .init = NULL,
    .destroy = NULL,
    .receive = write_http_response,
    .receive_ctrl = NULL,
    .route = default_routing,
    .deserialize = default_deserialize,
    .send_local = default_send_local,
    .send_remote = default_send_remote,
    .generate_id = NULL
};

