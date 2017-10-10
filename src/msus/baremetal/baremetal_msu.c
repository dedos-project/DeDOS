#include "baremetal/baremetal_socket_msu.h"
#include "baremetal/baremetal_msu.h"
#include "local_msu.h"
#include "rt_stats.h"
#include "msu_type.h"
#include "msu_message.h"
#include "logging.h"
#include "msu_calls.h"
#include "profiler.h"
#include "routing_strategies.h"
#include <stdlib.h>

static int receive(struct local_msu *self, struct msu_msg *msu_msg) {
    struct baremetal_msg *msg = msu_msg->data;
    msg->n_hops--;

    if (msg->n_hops == 0) {
        PROFILE_EVENT(msu_msg->hdr, PROF_DEDOS_EXIT);
        int rtn = close(msg->fd);
        if (rtn < 0) {
            log_error("Error closing file descriptor %d", msg->fd);
        }
        free(msg);
        return 0;
    }

    return call_msu_type(self, self->type, &msu_msg->hdr, sizeof(*msg), msg);
}

static int route(struct msu_type *type, struct local_msu *sender,
                 struct msu_msg *msg, struct msu_endpoint *output) {
    struct baremetal_msg *bm_msg = msg->data;
    if (bm_msg->n_hops <= 1) {
        return route_to_origin_runtime(type, sender, msg, output);
    }
    return default_routing(type, sender, msg, output);
}

struct msu_type BAREMETAL_MSU_TYPE = {
    .name = "Baremetal_Msu",
    .id = BAREMETAL_MSU_TYPE_ID,
    .receive = receive,
    .route = route
};

