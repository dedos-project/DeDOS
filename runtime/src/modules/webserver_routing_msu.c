#include "modules/webserver_routing_msu.h"
#include "logging.h"
#include "dedos_msu_list.h"
#include "generic_msu.h"
#include "generic_msu_queue_item.h"
#include "routing.h"
#include "msu_state.h"

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

#ifndef LOG_WEBSERVER_ROUTING
#define LOG_WEBSERVER_ROUTING 0
#endif

static int route_request(struct generic_msu *self,
                         struct generic_msu_queue_item *queue_item) {
    if (queue_item->buffer == NULL) {
        log_error("Cannot get file descriptor from queue_item");
        return -1;
    }
    struct webserver_queue_data *data = queue_item->buffer;
    int fd = data->fd;
    log_custom(LOG_WEBSERVER_ROUTING, "got routing request for fd %d", data->fd);

    size_t size = -1;

    struct webserver_state *state = msu_get_state(self, queue_item->id, 0, &size);
    if (state == NULL) {
        log_custom(LOG_WEBSERVER_ROUTING, "Initializing new state for id %d", queue_item->id);
        state = msu_init_state(self, queue_item->id, 0, sizeof(*state));
        state->fd = fd;
        init_connection_state(&state->conn_state, fd);
    } else if (size != sizeof(*state)) {
        log_warn("Got size (%d) that didn't match expected (%d)!", (int)size, (int) sizeof(*state));
    }

    if (state->fd != data->fd) {
        log_error("Got non-matching FDs! %d vs %d", state->fd, data->fd);
        return -1;
    }

    if (queue_item->msu_owner != self->id) {
        memcpy(state, queue_item->buffer, queue_item->buffer_len);
        log_custom(LOG_WEBSERVER_ROUTING, "Freeing queue item buffer");
        free(queue_item->buffer);
    }

    queue_item->buffer = (void*)state;
    queue_item->buffer_len = sizeof(*state);
    queue_item->msu_owner = self->id;

    switch (state->conn_state.conn_status) {
        case NO_CONNECTION:
        case CON_ACCEPTED:
        case CON_SSL_CONNECTING:
        case CON_READING:
            return DEDOS_WEBSERVER_READ_MSU_ID;
        case CON_PARSING:
        case CON_DB_CONNECTING:
        case CON_DB_SEND:
        case CON_DB_RECV:
            return DEDOS_WEBSERVER_HTTP_MSU_ID;
        case CON_WRITING:
            return DEDOS_WEBSERVER_WRITE_MSU_ID;
        case CON_COMPLETE:
        case CON_CLOSED:
            msu_free_state(self, queue_item->id, 0);
            return 0;
        case CON_ERROR:
            log_warn("Freeing state due to error");
            msu_free_state(self, queue_item->id, 0);
            return -1;
        default:
            log_error("Webserver routing MSU got unknown status (%d) for fd %d, id %l",
                      state->conn_state.conn_status, state->fd, queue_item->id);
            return -1;
    }
}

struct msu_endpoint this_endpoint = {
    .id = -1,
    .type_id = DEDOS_WEBSERVER_ROUTING_MSU_ID,
    .locality = MSU_IS_LOCAL,
    .msu_queue = NULL
};

struct msu_endpoint *this_endpoint_ref = NULL;
struct generic_msu *this_msu_ref = NULL;

static int ws_route_init(struct generic_msu *self, struct create_msu_thread_data *initial_state){
    if (this_endpoint_ref != NULL) {
        log_warn("Initializing more than one webserver route is likely a mistake");
    }
    this_endpoint.id = self->id;
    this_endpoint.msu_queue = &self->q_in;
    this_endpoint_ref = &this_endpoint;
    this_msu_ref = self;
    return 0;
}

struct generic_msu *webserver_routing_instance() {
    if (this_msu_ref == NULL) {
        log_error("Webserver route hasn't been initialized");
        return NULL;
    }
    return this_msu_ref;
}

struct msu_endpoint *webserver_routing_endpoint() {
    if (this_endpoint_ref == NULL) {
        log_error("Webserver route hasn't been initialized!");
        return NULL;
    }
    return this_endpoint_ref;
}

static struct msu_endpoint *webserver_routing_route_fn(
        struct msu_type UNUSED *type,
        struct generic_msu UNUSED *sender,
        struct generic_msu_queue_item UNUSED *queue_item) {
    return webserver_routing_endpoint();
}

const struct msu_type WEBSERVER_ROUTING_MSU_TYPE = {
    .name = "webserver_routing_msu",
    .layer = DEDOS_LAYER_APPLICATION,
    .type_id = DEDOS_WEBSERVER_ROUTING_MSU_ID,
    .proto_number = 999, //?
    .init = ws_route_init,
    .destroy = NULL,
    .receive = route_request,
    .receive_ctrl = NULL,
    .route = webserver_routing_route_fn,
    .deserialize = default_deserialize,
    .send_local = default_send_local,
    .send_remote = default_send_remote,
    .generate_id = NULL
};
