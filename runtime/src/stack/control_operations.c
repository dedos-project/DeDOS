#include <error.h>
#include "runtime.h"
#include "comm_protocol.h"
#include "dedos_msu_list.h"
#include "modules/msu_tcp_handshake.h"
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
    log_debug("Thread msg payload's buffer_len: %d",thread_msg->buffer_len);
    ret = dedos_thread_enqueue(d_thread, thread_msg);
    pthread_t self_id = pthread_self();
    log_debug("SELF ID: %lu", self_id);
    log_debug("Enqueued create_msu_request, thread_q->size: %d, thread_q->num_msgs %u",
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
    ret = dedos_thread_enqueue(d_thread, thread_msg);
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
    ret = dedos_thread_enqueue(d_thread, thread_msg);
    pthread_t self_id = pthread_self();
    log_debug("Enqueued msu request thread_q->size: %d, thread_q->num_msgs %d",
            ret, d_thread->thread_q->num_msgs);

    return ret;
}

static int process_create_msu_request(struct dedos_thread_msg *msg,
                                      struct dedos_thread *thread){
    if (!msg->data) {
        log_error("CREATE_MSU requires msg->data to be present and of type int");
        return -1;
    }

    struct create_msu_thread_data* create_action = msg->data;
    struct generic_msu *new_msu = init_msu(create_action->msu_type,
                                           create_action->msu_id,
                                           create_action);
    free(create_action->init_data);
    if (!new_msu){
        log_error("Could not initialize new MSU %d", create_action->msu_id);
        return -1;
    }
    log_info("Successfully created MSU with ID: %d", create_action->msu_id);

    if (!thread->msu_pool){
        log_error("No MSU pool to add MSU %d to", create_action->msu_id);
        return -1;
    }
    dedos_msu_pool_add(thread->msu_pool, new_msu);
    log_debug("Added MSU %d to pool", new_msu->id);
    return 0;
}

static int process_destroy_msu_request(struct dedos_thread_msg *msg,
                                       struct dedos_thread *thread){
    if (!msg->data) {
        log_error("DESTROY_MSU requires msg->data to be of type "
                  "create_msu_thread_data");
        return -1;
    }
    int msu_id = (int)msg->action_data;
    struct generic_msu* msu = dedos_msu_pool_find(thread->msu_pool, msu_id);

    if (!msu){
        log_error("Deletion of non-existing msu %d requested", msu_id);
        return -1;
    }

    log_debug("Destroying msu %d (type %s)", msu_id, msu->type->name);

    dedos_msu_pool_delete(thread->msu_pool, msu);

    destroy_msu(msu);

    log_info("Successfully destroyed MSU %d", msu_id);
    return 0;
}

static int process_route_change_request(struct dedos_thread_msg *msg,
                                        struct dedos_thread *thread){
    int msu_id = msg->action_data;
    struct manage_msu_control_payload *payload = msg->data;

    struct generic_msu *msu = dedos_msu_pool_find(thread->msu_pool, msu_id);

    if (!msu){
        log_error("Addition of route to nonexistent msu %d requested on thread %ld",
                   msu_id, thread->tid);
        return -1;
    }


    struct msu_action_thread_data* update_msg = malloc(sizeof(*update_msg));
    if (!update_msg){
        log_error("Failed to allocate route update msg");
        return -1;
    }

    update_msg->route_id = payload->route_id;
    update_msg->msu_id = msu_id;
    update_msg->action = msg->action;

    struct generic_msu_queue_item * update_queue_item =
            malloc(sizeof(*update_queue_item));

    if (!update_queue_item){
        log_error("Failed to allocate queue item for route change request");
        free(update_msg);
        return -1;
    }

    update_queue_item->buffer = update_msg;
    update_queue_item->buffer_len = sizeof(*update_msg);

    int rtn = generic_msu_queue_enqueue(&msu->q_control, update_queue_item);
    if (rtn < 0){
        log_error("Error enqueuing route update message for msu %d", msu->id);
        free(update_msg);
        free(update_queue_item);
        return -1;
    }
    log_debug("Enqueued route update message on msu %d (queue len: %d)",
              msu->id, rtn);
}

void process_control_updates(void)
{
    // Get the current dedos_thread
    pthread_t self_tid = pthread_self();
    int index = get_thread_index(self_tid);
    if (index < 0) {
        log_error("Couldnt find index for self in global all_threads");
        return;
    }

    struct dedos_thread *thread = &all_threads[index];
    if (!thread) {
        log_error("Invalid self index in global all_threads");
        return;
    }

    /* dequeue from its own thread queue */
    struct dedos_thread_msg *msg = dedos_thread_dequeue(thread->thread_q);
    if (!msg) {
        // log_debug("%s", "Empty queue");
        return;
    }

    log_debug("dequeued following control msg: \n"
              "\taction: %u\n"
              "\tmsu_id: %d\n"
              "\tbuffer_len: %u",
              msg->action, msg->action_data, msg->buffer_len);

    int rtn = -1;
    switch (msg->action){
        case CREATE_MSU:
            rtn = process_create_msu_request(msg, thread);
            break;
        case DESTROY_MSU:
            rtn = process_destroy_msu_request(msg, thread);
            break;
        case ADD_ROUTE_TO_MSU:
        case DEL_ROUTE_FROM_MSU:
            rtn = process_route_change_request(msg, thread);
            break;
        default:
            log_error("Unknown thread control action %d", msg->action);
    }
    if (rtn < 0){
        log_error("Error processing thread control queue");
    }
    log_critical("BADFIX: Sleeping in process control updates to keep ASAN happy with some race condition\n");
    sleep(5);
    // Free the thread message
    dedos_thread_msg_free(msg);
}
