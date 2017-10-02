#include "msu_message.h"
#include "local_msu.h"
#include "msu_calls.h"

#include "webserver/regex_routing_msu.h"
#include "webserver/regex_msu.h"

static int routing_receive(struct local_msu *self, struct msu_msg *msg) {
    return call_msu_type(self, &WEBSERVER_REGEX_MSU_TYPE, &msg->hdr, msg->data_size, msg->data);
}

struct msu_type WEBSERVER_REGEX_ROUTING_MSU_TYPE = {
    .name = "Regex_Routing_MSU",
    .id = WEBSERVER_REGEX_ROUTING_MSU_TYPE_ID,
    .receive = routing_receive
};
