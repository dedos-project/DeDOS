#include "pico_eth.h"
#include "pico_queue.h"
#include "pico_frame.h"
#include "pico_addressing.h"
#include "pico_tcp.h"
#include "runtime.h"
#include "modules/hs_request_routing_msu.h"
#include "modules/msu_tcp_handshake.h"
#include "pico_socket_tcp.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_list.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"
#include "chord.h"

int hs_request_routing_msu_process_queue_item(struct generic_msu *self,
        struct generic_msu_queue_item *queue_item)
{
    /* function called when an item is dequeued */
    /* queue_item can be parsed into the struct with is expected in the queue */
    struct routing_queue_item *rt_queue_item;
    struct pico_frame *f;
    int next_msu_id;
    struct chord_node* dst_chord_node;
    unsigned int derived_key;
    int ret;

    rt_queue_item = (struct routing_queue_item*) queue_item->buffer;
    f = rt_queue_item->f;
    log_debug("Dequeued following msg %s","");
    log_debug("\tqueue_item->buffer_len: %u",queue_item->buffer_len);
    log_debug("\trt_queue_item->src_msu_id: %d",rt_queue_item->src_msu_id);
    log_debug("\trt_queue_item->f: %s","");
    print_frame(rt_queue_item->f);

    struct msu_endpoint *tmp_dst_endpoint;
    struct msu_endpoint *all_msu_enpoints = NULL;
    return DEDOS_TCP_HANDSHAKE_MSU_ID;
/*
    //find an entry for routing msu from the routing table based on some info from frame f, like 4 tuple
    //compute a 32 bit key from 4 tuple
    struct pico_tcp_hdr *hdr = (struct pico_tcp_hdr *) (f->transport_hdr);
    struct pico_ipv4_hdr *net_hdr = (struct pico_ipv4_hdr *) f->net_hdr;

    derived_key = short_be(hdr->trans.sport);
    derived_key = (derived_key << 16) | short_be(hdr->trans.dport);
    derived_key = derived_key ^ net_hdr->src.addr;
    log_debug("Derived key for above frame: %u", derived_key);

    // do a lookup to get next_msu_id
    dst_chord_node = lookup_key(self->internal_state, derived_key);
    next_msu_id = dst_chord_node->next_msu_id;
    log_debug("Picked next HS MSU id: %d",next_msu_id);

    tmp_dst_endpoint = dst_chord_node->msu_endpoint;

    // refactor here, direct reference to msu_endpoint from chord node
    // all_msu_enpoints = get_all_type_msus(self->rt_table, next_msu_type);
    // if(!all_msu_enpoints){
    //     log_error("%s","No next MSU info...can't continue");
    //     return -1;
    // }
    //
    // tmp_dst_endpoint = get_msu_from_id(all_msu_enpoints, next_msu_id);
    // if(!tmp_dst_endpoint){
    //     log_error("Failed to get an endpoint for routing%s","");
    //     return -1;
    // }

    if(tmp_dst_endpoint->id != next_msu_id){
        log_error("Mismatch in id's given by chord (%d) and routing table entry (%d)",next_msu_id, tmp_dst_endpoint->id);
        return -1;
    }

    // forward packet to selected HS MSU, local or remote by creating a intermsu_msg with src = routing_item->src_msu_id
    if (is_endpoint_local(tmp_dst_endpoint) == 0) {
    	log_debug("Just forwarding the queue item to next hs msu's queue, next id: %d",tmp_dst_endpoint->id);
    	ret = generic_msu_queue_enqueue(tmp_dst_endpoint->next_msu_input_queue, queue_item);
        if(ret < 0){
            log_debug("Couldn't enqueue: %s","");
            free(rt_queue_item->f);
            free(rt_queue_item);
            delete_generic_msu_queue_item(queue_item);
            return -1;
        }
    } else {
        forward_to_next_msu(self, MSU_PROTO_TCP_HANDSHAKE, f->buffer,
            f->buffer_len, rt_queue_item->src_msu_id, tmp_dst_endpoint);
        free(rt_queue_item->f);
        free(rt_queue_item);
        delete_generic_msu_queue_item(queue_item);
    }

    return 0;
    */
}
/*
void hs_request_routing_msu_destroy(struct generic_msu *self)
{
    // any stuff which it must complete before getting destroyed can be done here 
    int id;
    log_debug("Destroying MSU with id: %u", self->id);
    if(self->q_in->shared){
        pico_mutex_destroy(self->q_in->mutex);
    }
    if(self->q_control_updates->shared){
        pico_mutex_destroy(self->q_control_updates->mutex);
    }
    free(self->name);
    free(self->q_in);
    free(self->q_control_updates);
    //TODO Free routing table!
    destroy_chord_ring(self->internal_state);
    free(self);
    id = self->id;
    log_debug("Destroyed msu id: %d",id);
}
*/

int hs_request_routing_msu_restore(struct generic_msu *self,
        struct dedos_intermsu_message* msg, void *buf, uint16_t bufsize)
{
    //parse the received buffer, then enqueue the request
    struct pico_frame *f;
    char *bufs = (char *) buf;
    int ret;

    if (msg->proto_msg_type == MSU_PROTO_TCP_ROUTE_HS_REQUEST) {
        log_debug("Received MSU_PROTO_TCP_ROUTE_HS_REQUEST from msu: %d",
                msg->src_msu_id);

        f = pico_frame_alloc(bufsize);
        memcpy(f->buffer, bufs, bufsize);

        f->buffer_len = bufsize;
        f->start = f->buffer;
        f->len = f->buffer_len;
        f->datalink_hdr = f->buffer;
        f->net_hdr = f->datalink_hdr + PICO_SIZE_ETHHDR;
        f->net_len = 20;
        f->transport_hdr = f->net_hdr + f->net_len;
        f->transport_len = (uint16_t) (f->len - f->net_len
                - (uint16_t) (f->net_hdr - f->buffer));

        //enqueue the request and also the src msu_id

        msu_queue_item *item = create_generic_msu_queue_item();
        if (!item) {
            log_error("Failed malloc for generic queue item %s", "");
        }
        struct routing_queue_item *queue_payload =
                (struct routing_queue_item*) malloc(
                        sizeof(struct routing_queue_item));
        if (!queue_payload) {
            log_error("Failed malloc for routing_queue_item %s", "");
            return -1;
        }
        queue_payload->f = f;
        queue_payload->src_msu_id = msg->src_msu_id;
        item->buffer = queue_payload;
        item->buffer_len = f->buffer_len + sizeof(struct routing_queue_item);

        ret = generic_msu_queue_enqueue(&self->q_in, item);
        log_debug("Enqueued MSU_PROTO_TCP_ROUTE_HS_REQUEST: q_size: %d", ret);
    } else {
        log_error("Unknown proto_msg_type %d", msg->proto_msg_type);
        return -1;
    }

    return 0;
}

/*
int hs_request_routing_msu_process_control_q_item(struct generic_msu *self, struct generic_msu_queue_item *control_q_item){
	int ret = 0;
	struct msu_control_update *update_msg;

	update_msg = control_q_item->buffer;

	//check if update was meant for me
    if(self->id != update_msg->msu_id){
    	log_error("I (msu_id %d) got update destined for msu %d",self->id, update_msg->msu_id);
    	goto end;
    }
    log_debug("Have an update with type: %u", update_msg->update_type);
    if(update_msg->update_type == MSU_ROUTE_ADD || update_msg->update_type == MSU_ROUTE_DEL){
    	//call function handle_routing_table_update
        int first_entry = 0;
        if(self->rt_table == NULL){
            first_entry = 1;
        }
    	self->rt_table = handle_routing_table_update(self->rt_table, update_msg);

        //also update chord ring (flow table)
        ret = update_flow_table(self, update_msg);
        if(ret){
            log_error("Failed to update flow table for msu id: %d",self->id);
            //TODO Rollback the routing table entry and notify global controller
            return -1;
        }
        if(DEBUG){
            log_debug("Chord ring for msu: %s, id: %d",self->name, self->id);
            traverse_ring(self->internal_state, DIR_CLOCKWISE);
        }

        if(!first_entry){
            if(update_msg->update_type == MSU_ROUTE_ADD){
                self->scheduling_weight++;
            } else if(update_msg->update_type == MSU_ROUTE_DEL) {
                self->scheduling_weight--;
                if(self->scheduling_weight == 0){
                    self->scheduling_weight = 1;
                }
            }
        }
        log_debug("New scheduling factor for msu %d, (%s): %u",self->id, self->name, self->scheduling_weight);
    }
    else{
    	log_error("Unknown update msg type %u",update_msg->update_type);
    	goto end;
    }

end:
    // Critical, must do these 
    free_msu_control_update(update_msg);
    free(control_q_item);
    log_debug("Returning from function %s","");
    return 0;
}
*/
struct msu_type HS_REQUEST_ROUTING_MSU_TYPE = {
    .name="hs_request_routing_msu",
    .layer=DEDOS_LAYER_TRANSPORT,
    .type_id=DEDOS_TCP_HS_REQUEST_ROUTING_MSU_ID,
    .proto_number=MSU_PROTO_TCP_ROUTE_HS_REQUEST,
    .init=NULL,
    .destroy=NULL,
    .receive=hs_request_routing_msu_process_queue_item,
    .receive_ctrl=NULL,
    .route=round_robin,
    .deserialize=hs_request_routing_msu_restore,
    .send_local=default_send_local,
    .send_remote=default_send_remote
};

