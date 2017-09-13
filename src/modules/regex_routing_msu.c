#include "modules/regex_routing_msu.h"
#include "modules/regex_msu.h"
#include "dedos_msu_list.h"
#include "dedos_msu_msg_type.h"
#include "modules/webserver_msu.h"

static int regex_routing_receive(struct generic_msu *self, struct generic_msu_queue_item *queue_item){
    return DEDOS_REGEX_MSU_ID;
}

const struct msu_type REGEX_ROUTING_MSU_TYPE = {
    .name="regex_routing_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_REGEX_ROUTING_MSU_ID,
    .proto_number=MSU_PROTO_REGEX,
    .init=NULL,
    .destroy=NULL,
    .receive=regex_routing_receive,
    .receive_ctrl=NULL,
    .route=default_routing,
    .deserialize=regex_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote
};
