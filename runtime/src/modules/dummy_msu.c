#include "runtime.h"
#include "dummy_msu.h"
#include "pico_socket_tcp.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"

int dummy_msu_receive(msu_t *msu, msu_queue_item_t *queue_item){
    /* function called when an item is dequeued */
    // Signal that next msu should also be DEDOS_DUMMY_MSU
    return DEDOS_DUMMY_MSU;
}

msu_type_t DUMMY_MSU_TYPE = {
    .name="dummy_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_DUMMY_MSU,
    .proto_number=0,
    .init=NULL,
    .destroy=NULL,
    .receive=dummy_msu_receive,
    .receive_ctrl=NULL,
    .route=round_robin,
    .deserialize=default_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote
};


