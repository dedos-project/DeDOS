/**
 * Example usage of MSU. Do not use.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "dedos_msu_list.h"
#include "runtime.h"
#include "modules/dummy_msu.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"

#ifdef __cplusplus
}
#endif

/**
 * Recieves data for the dummy msu
 * @param msu dummy msu to receive data
 * @param queue_item queue item to be recieved
 */
int dummy_msu_receive(local_msu *msu, msu_queue_item *queue_item){
    /* function called when an item is dequeued */
    // Signal that next msu should also be DEDOS_DUMMY_MSU
    return DEDOS_DUMMY_MSU_ID;
}

msu_type DUMMY_MSU_TYPE = {
    .name="dummy_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_DUMMY_MSU_ID,
    .proto_number=0,
    .init=NULL,
    .destroy=NULL,
    .receive=dummy_msu_receive,
    .receive_ctrl=NULL,
    .route=round_robin,
    .send_local=default_send_local,
    .send_remote=default_send_remote,
    .deserialize=default_deserialize
};


