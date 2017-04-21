/**
 * baremetal_msu.c
 *
 * MSU for evaluating runtime performance and mock computational
 * delay within MSU
 */
#include "runtime.h"
#include "modules/baremetal_msu.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_msu_list.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"
#include <pcre.h>

/** Deserializes data received from remote MSU and enqueues the
 * message payload onto the msu queue.
 *
 * NOTE: If there are substructures in the buffer to be received,
 *       a type-specific deserialize function will have to be
 *       implemented.
 *
 * TODO: I don't think "void *buf" is necessary.
 * @param self MSU to receive data
 * @param msg remote message to be received, containing msg->payload
 * @param buf ???
 * @param bufsize ???
 * @return 0 on success, -1 on error
 */
static int baremetal_deserialize(struct generic_msu *self, intermsu_msg *msg,
                        void *buf, uint16_t bufsize){
    if (self){
        msu_queue_item *recvd =  malloc(sizeof(*recvd));
        if (!(recvd)){
            log_error("Could not allocate msu_queue_item");
            return -1;
        }

        struct baremetal_msu_data_payload *baremetal_data = malloc(sizeof(*baremetal_data));
        if (!baremetal_data){
            free(recvd);
            log_error("Could not allocate baremetal_msu_data_payload");
            return -1;
        }
        memcpy(baremetal_data, msg->payload, sizeof(*baremetal_data));

        recvd->buffer_len = sizeof(*baremetal_data);
        recvd->buffer = baremetal_data;
        generic_msu_queue_enqueue(&self->q_in, recvd);
        return 0;
    }
    return -1;
}


/**
 * Recieves data for baremetal MSU
 * @param self baremetal MSU to receive the data
 * @param input_data contains a baremetal_msu_data_payload* in input_data->buffer
 * @return ID of next MSU-type to receive data, or -1 on error
 */
int baremetal_receive(struct generic_msu *self, msu_queue_item *input_data) {
    if (self && input_data) {
        struct baremetal_msu_data_payload *baremetal_data =
            (struct baremetal_msu_data_payload*) (input_data->buffer);
        int ret;

        log_warn("TODO");
    }
    return -1;
}

int baremetal_msu_init_entry(struct generic_msu *self,
        struct create_msu_thread_msg_data *create_action)
{
    /* any other internal state that MSU needs to maintain */
    //For routing MSU the internal state will be the chord ring
    if(self->id == 1 && baremetal_entry_msu == NULL){
        baremetal_entry_msu = self;
    }
    log_debug("Set baremetal entry msu to be MSU with id: %u",self->id);
    return 0;
}
/**
 * All baremetal MSUs contain a reference to this type
 */
const struct msu_type BAREMETAL_MSU_TYPE = {
    .name="baremetal_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_BAREMETAL_MSU_ID,
    .proto_number=MSU_PROTO_NONE,
    .init=baremetal_msu_init_entry,
    .destroy=NULL,
    .receive=baremetal_receive,
    .receive_ctrl=NULL,
    .route=round_robin,
    .deserialize=baremetal_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote,
};
