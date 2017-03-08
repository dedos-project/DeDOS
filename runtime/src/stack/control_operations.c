#include <error.h>
#include "runtime.h"
#include "comm_protocol.h"
#include "dedos_msu_list.h"
#include "msu_tcp_handshake.h"
#include "communication.h"
#include "control_msg_handler.h"
#include "control_protocol.h"
#include "control_operations.h"
#include "dedos_thread_queue.h"
#include "logging.h"
#include "dedos_msu_pool.h"
#include "routing.h"
#include "generic_msu.h"
#include "dedos_statistics.h"

int create_msu_request(struct dedos_thread *d_thread,
        struct dedos_thread_msg* thread_msg)
{
    //Enqueue the request in the particular thread mentioned by first arg
    int ret;
    if (!d_thread || !thread_msg) {
        log_error("Empty parameter ptrs for do create msu request.%s", "");
        return -1;
    }
    //Enqueue the request
    log_debug("Before thread enqueue.....%s","");
    ret = dedos_thread_enqueue(d_thread->thread_q, thread_msg);
    pthread_t self_id = pthread_self();
    log_debug("SELF ID: %lu", self_id);
    log_debug("Enqueued create_msu_request, thread_q->size: %d, thread_q->num_msgs %d",
            ret, d_thread->thread_q->num_msgs);

    return ret;
}

int delete_msu_request(struct dedos_thread *d_thread,
        struct dedos_thread_msg* thread_msg)
{
    int ret;
    if (!d_thread || !thread_msg) {
        log_error("Empty parameter ptrs for do delete msu request. %s", "");
        return -1;
    }
    //Enqueue the delete request
    ret = dedos_thread_enqueue(d_thread->thread_q, thread_msg);
    pthread_t self_id = pthread_self();
    log_debug("Enqueued delete_msu_request, thread_q->size: %d, thread_q->num_msgs %d",
            ret, d_thread->thread_q->num_msgs);

    return ret;
}

int enqueue_msu_request(struct dedos_thread *d_thread,
        struct dedos_thread_msg* thread_msg)
{
    int ret;
    if (!d_thread || !thread_msg) {
        log_error("Empty parameter ptrs for do delete msu request. %s", "");
        return -1;
    }
    //Enqueue the delete request
    ret = dedos_thread_enqueue(d_thread->thread_q, thread_msg);
    pthread_t self_id = pthread_self();
    log_debug("Enqueued msu request thread_q->size: %d, thread_q->num_msgs %d",
            ret, d_thread->thread_q->num_msgs);

    return ret;
}

void process_control_updates(void)
{
    int index;
    struct dedos_thread *self;
    struct dedos_thread_msg *msg;
    struct generic_msu *tmp;
    struct msu_control_update *update_msg;
    struct generic_msu_queue_item *update_queue_item;
    int ret;
    pthread_t self_tid;
    self_tid = pthread_self();

    index = get_thread_index(self_tid);
    if (index < 0) {
        log_error("Couldnt find index in global all_threads %s", "OOPS!");
        return;
    }

    self = &all_threads[index];
    if (!self) {
        log_error("Couldnt find self in global all_threads %s", "OOPS!");
        return;
    }

    // if (self->thread_q->num_msgs && self->thread_q->num_msgs < 1) {
    //     log_debug("No control update messages in thread %u", self->tid);
    //     return;
    // }

    /* dequeue from its own thread queue */
    msg = dedos_thread_dequeue(self->thread_q);
    if (!msg) {
        // log_debug("%s", "Empty queue");
        return;
    }
    log_debug("dequeued following control msg: " "\n\taction: %u \n\tmsu_id: %d\n\tbuffer_len: %u",
        msg->action, msg->action_data, msg->buffer_len);

    /* process the thread msg by calling appropriate action handlers */
    if (!msg->action) {
        log_error("%s", "No action code present...!");
        goto end;
    }

    if (msg->action == CREATE_MSU) {
        if (!msg->data) {
            log_error(
                    "CREATE_MSU requires create_msu_thread_msg_data to be present in *data %s",
                    "");
            goto end;
        }

        struct create_msu_thread_msg_data* create_action;
        create_action = msg->data;
        log_debug("Init data len: %d",create_action->init_data_len);
        tmp = init_msu(create_action->msu_type, msg->action_data,
                       create_action);
        if(!tmp){
            free(create_action->creation_init_data);
            return;
        }
        log_info("Successfully created MSU with ID: %d", msg->action_data);

        //add to msu_pool
        if(!self->msu_pool){
            log_error("%s","No msu pool!");
            free(create_action->creation_init_data);
            return;
        }
        dedos_msu_pool_add(self->msu_pool, tmp);
        log_debug("Added MSU to pool, msu id: %d", tmp->id);
        register_msu_with_thread_stats(self->thread_stats, tmp->id);
        log_debug("Added MSU to thread_stats, msu id: %d", tmp->id);

        free(create_action->creation_init_data);
    }
    else if (msg->action == DESTROY_MSU) {

        if (!msg->data) {
            log_error("DESTROY_MSU requires msu_type as *data %s", "");
            goto end;
        }

        int msu_type = msg->data;
        struct generic_msu* m;
        m = dedos_msu_pool_find(self->msu_pool, msg->action_data);
        if(!m){
            log_error("Provided MSU id doesn't exist %d",msg->action_data);
            goto end;
        }
        log_debug("Found generic msu in msu_pool id: %d, name: %s",msg->action_data, m->type->name);

        destroy_msu(m);

        //delete from msu_pool
        dedos_msu_pool_delete(self->msu_pool, m);
        remove_msu_thread_stats(self->thread_stats, msg->action_data);

        log_info("Successfully destroyed MSU with ID: %d", msg->action_data);
    }
    else if(msg->action == MSU_ROUTE_ADD || msg->action == MSU_ROUTE_DEL) {

        struct generic_msu* m;
        struct msu_control_add_route *msu_add_update_msg;
        struct msu_control_del_route *msu_del_update_msg;

        m = dedos_msu_pool_find(self->msu_pool, msg->action_data);
        if(!m){
            log_error("Provided MSU id doesn't exist %d",msg->action_data);
            goto end;
        }
        log_debug("Found generic msu in msu_pool id: %d, name: %s",msg->action_data, m->type->name);

        //create and fill up msu_control_update struct
        // this struct will be enqueued by thread into MSU control queue
        update_msg = (struct msu_control_update *)malloc(sizeof(struct msu_control_update));
        if(!update_msg){
        	log_error("Failed to malloc msu_control_update %s","");
        	goto end;
        }

        update_msg->msu_id = msg->action_data;
        update_msg->update_type = msg->action;

        //create and fill msu_control_update struct payload (msu_control_add_route or msu_control_del_route)
        if(msg->action == MSU_ROUTE_ADD){
        	msu_add_update_msg = (struct msu_control_add_route*)malloc(sizeof(struct msu_control_add_route));
            if(!msu_add_update_msg){
             	log_error("Failed to malloc msu_add_update_msg %s","");
             	free(update_msg);
             	goto end;
            }
            msu_add_update_msg->peer_ipv4 = 0;
            log_debug("thread_msg msg->data %s",(char*)(msg->data));
            memcpy(msu_add_update_msg, msg->data, msg->buffer_len);
            log_debug("Enqueuing add_update to MSU: %d", update_msg->msu_id);
            log_debug("\tpeer_msu_id: %d", msu_add_update_msg->peer_msu_id);
            log_debug("\tpeer_msu_type: %u", msu_add_update_msg->peer_msu_type);
            log_debug("\tpeer_locality: %u", msu_add_update_msg->peer_locality);
            log_debug("\tpeer_ip: %u", msu_add_update_msg->peer_ipv4);
            char ipstr[30];
            ipv4_to_string(&ipstr, msu_add_update_msg->peer_ipv4);
            log_debug("\tpeer_ip_str: %s", ipstr);
            //add pointer to update_msg
			update_msg->payload = msu_add_update_msg;
			update_msg->payload_len = sizeof(struct msu_control_add_route);

        }else if(msg->action == MSU_ROUTE_DEL){
        	msu_del_update_msg = (struct msu_control_del_route*)malloc(sizeof(struct msu_control_del_route));
            if(!msu_del_update_msg){
             	log_error("Failed to malloc msu_del_update_msg %s","");
             	free(update_msg);
             	goto end;
            }
            msu_del_update_msg->peer_ipv4 = 0;
            log_debug("thread_msg msg->data %s",msg->data);
            memcpy(msu_del_update_msg, msg->data, msg->buffer_len);
            log_debug("Enqueuing del_update to MSU: %d", update_msg->msu_id);
            log_debug("\tpeer_msu_id: %d", msu_del_update_msg->peer_msu_id);
            log_debug("\tpeer_msu_type: %u", msu_del_update_msg->peer_msu_type);
            log_debug("\tpeer_locality: %u", msu_del_update_msg->peer_locality);
            log_debug("\tpeer_ip: %u", msu_del_update_msg->peer_ipv4);
            char ipstr[30];
            ipv4_to_string(&ipstr, msu_del_update_msg->peer_ipv4);
            log_debug("\tpeer_ip_str: %s", ipstr);

            //add pointer to update_msg
            update_msg->payload = msu_del_update_msg;
            update_msg->payload_len = sizeof(struct msu_control_del_route);
        }

        //if all was good above then create a generic msu_queue item
        update_queue_item = (struct generic_msu_queue_item *)malloc(sizeof(struct generic_msu_queue_item));
        if(!update_queue_item){
         	log_error("Failed to malloc update generic_queue_item %s","");
          	free(update_msg);
            if(msg->action == MSU_ROUTE_ADD){
            	free(msu_add_update_msg);
            }else if(msg->action == MSU_ROUTE_DEL){
            	free(msu_del_update_msg);
            }
            goto end;
        }

       	update_queue_item->buffer = update_msg;
        update_queue_item->buffer_len = sizeof(struct msu_control_update) + update_msg->payload_len;

        //enqueue in msu's control data queue.
        ret = generic_msu_queue_enqueue(&m->q_control, update_queue_item);
        log_debug("Successfully enqueued update msg for msu: %d", msg->action_data);
        log_debug("MSU %d control queue size: %d", msg->action_data, ret);
    }
    else {
        log_error("Unknown control action %u", msg->action);
    }
    //free the dequeued dedos_thread_msg
    end: dedos_thread_msg_free(msg);
    // log_debug("DEBUG: Freed thread msg %s","");
}
