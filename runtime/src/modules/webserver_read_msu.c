#include "modules/webserver_read_msu.h"
#include "modules/webserver_routing_msu.h"
#include "modules/blocking_socket_handler_msu.h"
#include "logging.h"
#include "dedos_msu_list.h"
#include "generic_msu.h"
#include "generic_msu_queue_item.h"
#include "routing.h"

struct ws_read_state {
    int use_ssl;
};

#ifndef LOG_WEBSERVER_READ
#define LOG_WEBSERVER_READ 0
#endif

static int read_request(struct generic_msu *self,
                        struct generic_msu_queue_item *queue_item) {
    struct ws_read_state *read_state = self->internal_state;

    struct webserver_state *ws_state = queue_item->buffer;
    log_custom(LOG_WEBSERVER_READ, "got read request for fd %d", ws_state->fd);
    int use_ssl = read_state->use_ssl;
    if (ws_state == NULL) {
        log_error("Cannot read request with NULL state! How did this happen?");
        return -1;
    }
    struct connection_state *state = &ws_state->conn_state;

    int rtn;
    switch (state->conn_status) {
        case NO_CONNECTION:
        case CON_ACCEPTED:
        case CON_SSL_CONNECTING:
            log_custom(LOG_WEBSERVER_READ, "Got CON_SSL_CONNECTING");
            if (use_ssl) {
                rtn = accept_connection(state, use_ssl);
                if (rtn < 0) {
                    // So state can be cleared
                    log_custom(LOG_WEBSERVER_READ, "accept failed for fd %d", ws_state->fd);
                    return DEDOS_WEBSERVER_ROUTING_MSU_ID;
                }
                if (state->conn_status != CON_READING) {
                    monitor_fd(state->fd, EPOLLIN);
                    return 0;
                } // else: continue to next case (CON_READING)
            }
        case CON_READING:
            log_custom(LOG_WEBSERVER_READ, "Got CON_READING");
            rtn = read_connection(state);
            if (rtn < 0) {
                log_custom(LOG_WEBSERVER_READ, "read failed for fd %d", ws_state->fd);
                // So state can be cleared
                return DEDOS_WEBSERVER_ROUTING_MSU_ID;
            }
            if (state->conn_status != CON_PARSING) {
                monitor_fd(state->fd, EPOLLIN);
                return 0;
            }
            return DEDOS_WEBSERVER_HTTP_MSU_ID;
        default:
            log_error("MSU %d received queue item with improper status: %d",
                      self->id, state->conn_status);
            return -1;
    }
}

#define SSL_INIT_CMD "SSL"

static int ws_read_init(struct generic_msu *self,
                        struct create_msu_thread_data *initial_state) {

    struct ws_read_state *ws_state = malloc(sizeof(*ws_state));

    char *init_cmd = initial_state->init_data;

    if (init_cmd == NULL) {
        log_info("Initializing NON-SSL webserver-reading MSU");
        ws_state->use_ssl = 0;
    }
    if (strncasecmp(init_cmd, SSL_INIT_CMD, sizeof(SSL_INIT_CMD)) == 0) {
        log_info("Initializing SSL webserver-reading MSU");
        ws_state->use_ssl = 1;
    } else {
        ws_state->use_ssl = 0;
    }

    self->internal_state = (void*)ws_state;
    return 0;
}

static void ws_read_destroy(struct generic_msu *self) {
    free(self->internal_state);
}

const struct msu_type WEBSERVER_READ_MSU_TYPE = {
    .name = "webserver_routing_msu",
    .layer = DEDOS_LAYER_APPLICATION,
    .type_id = DEDOS_WEBSERVER_READ_MSU_ID,
    .proto_number = 999, //?
    .init = ws_read_init,
    .destroy = ws_read_destroy,
    .receive = read_request,
    .receive_ctrl = NULL,
    .route = default_routing,
    .deserialize = default_deserialize,
    .send_local = default_send_local,
    .send_remote = default_send_remote,
    .generate_id = NULL
};
