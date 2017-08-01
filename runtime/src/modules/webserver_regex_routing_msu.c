#include "modules/webserver_regex_routing_msu.h"
#include "dedos_msu_list.h"

static int routing_receive(struct generic_msu *self, struct generic_msu_queue_item *queue_item) {
    return DEDOS_WEBSERVER_REGEX_MSU_ID;
}

const struct msu_type WEBSERVER_REGEX_ROUTING_MSU_TYPE = {
    .name="regex_routing_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_WEBSERVER_REGEX_ROUTING_MSU_ID,
    .proto_number=999, //?
    .init=NULL,
    .destroy=NULL,
    .receive=routing_receive,
    .receive_ctrl=NULL,
    .route=default_routing,
    .deserialize=default_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote
};
