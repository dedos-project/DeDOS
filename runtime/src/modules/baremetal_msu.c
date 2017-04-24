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
#include <errno.h>

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

static int baremetal_mock_delay(struct generic_msu *self, struct baremetal_msu_data_payload *baremetal_data){
    log_debug("TODO: Can simulate delay here for each baremetal msu");
    return 0;
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
        char buffer[BAREMETAL_RECV_BUFFER_SIZE];
        log_debug("Baremetal receive for msu: %d, msg type: %d",self->id, baremetal_data->type);
        if(baremetal_data->type == READ_FROM_SOCK){
            //read from socket//only happens at entry msu
            log_debug("READ_FROM_SOCK state");
            ret = recv(baremetal_data->socketfd, &buffer, BAREMETAL_RECV_BUFFER_SIZE, MSG_WAITALL);
            if(ret > 0){
                sscanf(buffer, "%d", &(baremetal_data->int_data));
                log_debug("Received int from client: %d",baremetal_data->int_data);
            } else if(ret == 0){
                log_debug("Socket closed by peer");
                return -1;
            } else {
                log_debug("Error in recv: %s",strerror(errno));
                return -1;
            }
            baremetal_data->type = FORWARD;
        } else if(baremetal_data->type == FORWARD){
            baremetal_data->int_data += 1;
            log_debug("FORWARD state, data val: %d",baremetal_data->int_data);
            //Maybe check that if my routing table is empty for next type,
            //that means I am the last sink MSU and should send out the final response?
            struct msu_type *type = &BAREMETAL_MSU_TYPE;
            struct msu_endpoint *dst = get_all_type_msus(self->rt_table, type->type_id);
            if (dst == NULL){
                log_error("EXIT: No destination endpoint of type %s (%d) for msu %d so an exit!",
                      type->name, type->type_id, self->id);
                //Send call
                ret = send(baremetal_data->socketfd, &baremetal_data->int_data,
                        sizeof(baremetal_data->int_data),0);
                if(ret == -1){
                    log_error("Failed to send out data on socket: %s",strerror(errno));
                } else {
                    log_debug("Sent baremetal response bytes to client: %d",ret);
                }
            }
            return -1; //since nothing to forward
        }
        return DEDOS_BAREMETAL_MSU_ID;
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
