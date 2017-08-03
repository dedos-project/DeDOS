#include "modules/webserver_read_msu.h"
#include "modules/blocking_socket_handler_msu.h"
#include "modules/webserver/connection-handler.h"
#include "communication.h" // for runtimeIpAddress
#include "logging.h"
#include "dedos_msu_list.h"
#include "msu_state.h"
#include "generic_msu.h"
#include "generic_msu_queue_item.h"
#include "routing.h"

struct ws_read_state {
    int use_ssl;
};

#ifndef LOG_WEBSERVER_READ
#define LOG_WEBSERVER_READ 0
#endif

static int handle_read(struct read_state *read_state,
                       struct ws_read_state *msu_state,
                       struct generic_msu *self,
                       struct generic_msu_queue_item *queue_item) {
    log_custom(LOG_WEBSERVER_READ, "Attempting read from %p", read_state);
    int rtn = read_request(read_state);
    if (rtn & (WS_INCOMPLETE_WRITE | WS_INCOMPLETE_READ)) {
        read_state->conn.status = CON_READING;
        log_custom(LOG_WEBSERVER_READ, "Read incomplete. Re-enabling (fd: %d)", read_state->conn.fd);
        monitor_fd(read_state->conn.fd, RTN_TO_EVT(rtn), self);
        return 0;
    } else if (rtn & WS_ERROR) {
        close_connection(&read_state->conn);
        read_state->req_len = -1;
    } else {
        log_custom(LOG_WEBSERVER_READ, "Read %s", read_state->req);
    }
    struct read_state *out = malloc(sizeof(*out));
    memcpy(out, read_state, sizeof(*read_state));
    free(queue_item->buffer);
    queue_item->buffer = out;
    msu_free_state(self, queue_item->id, 0);
    return DEDOS_WEBSERVER_HTTP_MSU_ID;
}

static int handle_accept(struct read_state *read_state,
                         struct ws_read_state *msu_state,
                         struct generic_msu *self,
                         struct generic_msu_queue_item *queue_item) {
    int rtn = accept_connection(&read_state->conn, msu_state->use_ssl);
    if (rtn & WS_COMPLETE) {
        rtn = handle_read(read_state, msu_state, self, queue_item);
        return rtn;
    } else if (rtn & WS_ERROR) {
        close_connection(&read_state->conn);
        msu_free_state(self, queue_item->id, 0);
        return -1;
    } else {
        read_state->conn.status = CON_SSL_CONNECTING;
        monitor_fd(read_state->conn.fd, RTN_TO_EVT(rtn), self);
        return 0;
    }
}


static int read_http_request(struct generic_msu *self,
                        struct generic_msu_queue_item *queue_item) {
    struct ws_read_state *msu_state = self->internal_state;

    struct connection *conn_in = queue_item->buffer;
    int fd = conn_in->fd;

    size_t size = -1;
    struct read_state *read_state = msu_get_state(self, queue_item->id, 0, &size);
    if (read_state == NULL) {
        read_state = msu_init_state(self, queue_item->id, 0, sizeof(*read_state));
        init_read_state(read_state, conn_in);
        if (conn_in->ssl != NULL) {
            read_state->conn.status = CON_READING;
        }
    } else {
        log_custom(LOG_WEBSERVER_READ, "Retrieved read ptr %p", read_state);
    }
    if (read_state->conn.fd != fd) {
        log_error("Got non-matching FDs! %d vs %d", read_state->conn.fd, fd);
        return -1;
    }

    switch (read_state->conn.status) {
        case NIL:
        case NO_CONNECTION:
        case CON_ACCEPTED:
        case CON_SSL_CONNECTING:
            return handle_accept(read_state, msu_state, self, queue_item);
        case CON_READING:
            return handle_read(read_state, msu_state, self, queue_item);
        default:
            log_error("Received unknown packet status: %d", read_state->conn.status);
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

static struct msu_endpoint *default_within_ip_routing(struct msu_type *type,
                                     struct generic_msu *sender,
                                     struct generic_msu_queue_item *queue_item) {
    uint32_t origin_ip = queue_item->path[0].ip_address;
    struct msu_endpoint *dest = default_routing(type, sender, queue_item);
    if (dest->ipv4 == origin_ip ||
            (origin_ip == runtimeIpAddress && dest->locality == MSU_IS_LOCAL)) {
        return dest;
    }
    dest = round_robin_within_ip(type, sender, origin_ip);
    if (dest == NULL) {
        log_error("can't find appropriate webserver read MSU");
    }
    return dest;
}

const struct msu_type WEBSERVER_READ_MSU_TYPE = {
    .name = "webserver_routing_msu",
    .layer = DEDOS_LAYER_APPLICATION,
    .type_id = DEDOS_WEBSERVER_READ_MSU_ID,
    .proto_number = 999, //?
    .init = ws_read_init,
    .destroy = ws_read_destroy,
    .receive = read_http_request,
    .receive_ctrl = NULL,
    .route = default_within_ip_routing,
    .deserialize = default_deserialize,
    .send_local = default_send_local,
    .send_remote = default_send_remote,
    .generate_id = NULL
};
