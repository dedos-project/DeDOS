/**
 * @file generic_msu.c
 *
 *
 * Contains function for the creation of MSUs, as well as the
 * registration of new MSU types
 */

#include <stdlib.h>
#include "stats.h"
#include "generic_msu.h"
#include "runtime.h"
#include "routing.h"
#include "logging.h"
#include "dedos_statistics.h"

// TODO: This type-registration should probably be handled elsewhere
//       probably in msu_tracker?

/** Maximum number of MSU types that can be registered */
#define MAX_MSU_TYPES 64
/** When msu types are registered, they are included in this array */
static struct msu_type *msu_types = NULL;
/** Number of currently-registered msu types */
static unsigned int n_types = 0;

/**
 * Tracks the amount of memory allocated to an MSU.
 * Private function
 * @param msu msu in which to track allocation
 * @param bytes number of bytes to malloc
 * @returns pointer to the allocated memory
 */
static void *msu_track_alloc(struct generic_msu *msu, size_t bytes){
    void *ptr = NULL;
    ptr = malloc(bytes);
    if (!ptr) {
        log_error("Failed to allocate %d bytes for MSU id %d, %s",
            (int)bytes, msu->id, msu->type->name);
    } else {
        msu->stats.memory_allocated += bytes;
        log_debug("Successfully allocated %d bytes for MSU id %d, %s memory footprint: %u bytes",
                  (int)bytes, msu->id, msu->type->name, msu->stats.memory_allocated);
    }
    return ptr;
}

/**
 * Tracks the amount of memory allocated to an MSU.
 * Private function
 * @param ptr memory to free
 * @param msu msu in which to track allocatioin
 * @param bytes number of bytes being freed
 */
static void msu_track_free(void* ptr, struct generic_msu * msu, size_t bytes)
{
    msu->stats.memory_allocated -= bytes;
    log_debug("Freeing %u bytes used by MSU id %d, %s, memory footprint: %u bytes",
              (int)bytes, msu->id, msu->type->name, msu->stats.memory_allocated);
    free(ptr);
}

/**
 * Registers an MSU type so that it can be later referenced by its ID.
 *
 * Adds the type to the static *msu_types structure.
 * If more than 64 different types of MSUs are registered, this will fail.
 * @param type MSU type to be registered.
 */
void register_msu_type(struct msu_type *type){
    // TODO: realloc if n_types+1 == MAX
    if (msu_types == NULL)
        msu_types = malloc(sizeof(*msu_types) * MAX_MSU_TYPES);
    msu_types[n_types] = *type;
    log_debug("Registered MSU type: %s",type->name);
    n_types++;
}

/**
 * Gets a registered msu_type structure that matches the provided id.
 *
 * @param type_id numerical ID of the MSU type to be retrieved
 * @return first msu_type in which type->type_id matches the provided id or NULL if N/A
 */
struct msu_type *msu_type_by_id(unsigned int type_id){
    struct msu_type *type = NULL;
    // NOTE: This will require slightly more time than a switch statement
    for (int i=0; i<n_types; i++){
        if (msu_types[i].type_id == type_id){
            type = &msu_types[i];
            break;
        }
    }
    return type;
}

/**
 * Called on receipt of a new message from the global controller by an MSU.
 * Adds new routes and updates the scheduling weight if appropriate.
 * Calls individual MSU type's receive_ctrl() if not NULL
 * Automatically frees the queue_item and queue_item->buffer once processed.
 *
 * @param self MSU receiving the control message
 * @param queue_item control queue message
 * @return 0 on success, -1 on error
 */
int msu_receive_ctrl(struct generic_msu *self, msu_queue_item *queue_item){
    struct msu_control_update *update_msg = queue_item->buffer;
    int rtn = 0;
    if (self->id != update_msg->msu_id){
        log_error("ERROR: MSU %d got updated destined for MSU %d", self->id, update_msg->msu_id);
        rtn = -1;
    } else {
        log_debug("MSU %d got update with type %u", self->id, update_msg->update_type);
        int handled = 0;
        if (update_msg->update_type == MSU_ROUTE_ADD || update_msg->update_type == MSU_ROUTE_DEL){
            int first_entry = (self->rt_table == NULL);
            self->rt_table = handle_routing_table_update(self->rt_table, update_msg);
            handled = 1;
            if (!first_entry){
                if (update_msg->update_type == MSU_ROUTE_ADD){
                    self->scheduling_weight++;
                } else {
                    self->scheduling_weight--;
                    if (self->scheduling_weight == 0){
                        self->scheduling_weight = 1;
                    }
                }
            }
        }
        if (self->type->receive_ctrl){
            handled = (self->type->receive_ctrl(self, update_msg)) == 0;
        }

        if (!handled){
            log_error("Unknown update msg type %u received by MSU %d",
                    update_msg->update_type, self->id);
            rtn = -1;
        }
    }

    free_msu_control_update(update_msg);
    free(queue_item);
    log_debug("Freed update_msg and control_q_item %s","");
    return rtn;
}

/**
 *  Hidden data structure to protect access to MSU specific data.
 *  Data should only ever be allocated to this structure via:
 *      msu_data_alloc()
 *  and only freed via
 *      msu_data_free()
 */
struct msu_data{
    size_t n_bytes;
    void *data;
};

/** Allocates data in the msu and tracks the amount of allocated data.
 * Returns a pointer to the allocated data.
 * NOTE: MSU can only have a single allocated data structure at any time
 *       Calling this function again will realloc that data, and
 *       may cause unexpected behavior
 * TODO: Provide ID by which allocated data can be retrieved
 * @param msu MSU in which to track the allocated data
 * @param bytes number of bytes to malloc
 * @return pointer to the allocated data, or NULL if memory could not be allocated
 */
void *msu_data_alloc(struct generic_msu * msu, size_t bytes)
{
    void *ptr = realloc(msu->data_p->data, bytes);
    if (ptr == NULL){
        log_error("Failed to allocate %d bytes for MSU id %d, %s",
                  (int)bytes, msu->id, msu->type->name);
    } else {
        msu->stats.memory_allocated += (bytes - msu->data_p->n_bytes);
        log_debug("Successfully allocated %d bytes for MSU id %d, "
                  "%s memory footprint: %u bytes",
                  (int)bytes, msu->id, msu->type->name, msu->stats.memory_allocated);
        msu->data_p->n_bytes = bytes;
        msu->data_p->data = ptr;
    }
    return ptr;
}

/**
 * Frees data stored within an MSU and tracks that the data has been freed.
 * @param msu MSU in which to track the freed data
 */
void msu_data_free(struct generic_msu *msu)
{
    msu->stats.memory_allocated -= msu->data_p->n_bytes;
    log_debug("Freeing %u bytes used by MSU id %d, %s, memory footprint: %u bytes",
              (int)msu->data_p->n_bytes, msu->id, msu->type->name, msu->stats.memory_allocated);
    free(msu->data_p->data);
}

/**
 *  Gets the data allocated within an MSU.
 *  @return pointer to the allocated data
 */
void *msu_data(struct generic_msu *msu)
{
    return msu->data_p->data;
}

/**
 * Private function to allocate a new MSU.
 * MSU creation should be handled through init_msu()
 */
struct generic_msu *msu_alloc(void)
{
    struct generic_msu *msu = malloc(sizeof(*msu));
    if (msu == NULL){
        log_error("%s", "Failed to allocate msu");
        return msu;
    }

    msu->stats.memory_allocated = 0;
    msu->stats.queue_item_processed = 0;

    msu->data_p = malloc(sizeof(struct msu_data));
    msu->data_p->n_bytes = 0;
    msu->data_p->data = NULL;

    msu->q_in.mutex = mutex_init();
    msu->q_in.shared = 1;
    generic_msu_queue_init(&msu->q_in);

    msu->q_control.mutex = mutex_init();
    msu->q_control.shared = 1;
    generic_msu_queue_init(&msu->q_control);
    return msu;
}

/**
 * Private function to free an MSU.
 * MSU destruction should be handled through destroy_msu()
 */
void msu_free(struct generic_msu* msu)
{
    free(msu->data_p);
    free(msu);
}

/**
 * Sends a message to the main thread queue indicating that
 * the requested MSU has failed to be created.
 */
int msu_failure(int msu_id){
    struct dedos_thread_msg *thread_msg = dedos_thread_msg_alloc();
    if (thread_msg){
        thread_msg->action = FAIL_CREATE_MSU;
        thread_msg->action_data = msu_id;
        thread_msg->buffer_len = 0;
        thread_msg->data = NULL;
        dedos_thread_enqueue(main_thread->thread_q, thread_msg);
        return 0;
    }
    log_error("Failed to enqueue FAIL_CREATE_MSU message on main thread");
    return -1;
}


/**
 * Malloc's and creates a new MSU of the specified type and id.
 *
 * @param type_id ID of the MSU type to be created
 * @param msu_id ID of the instance of the MSU to be created
 * @param create_action Initial data to be provided to the MSU
 * @return initialized MSU or NULL if error occurred
 */
struct generic_msu *init_msu(unsigned int type_id, int msu_id,
                struct create_msu_thread_msg_data *create_action){
    if (type_id < 1){
        return NULL;
    }

    struct generic_msu *msu = msu_alloc();
    msu->id = msu_id;
    msu->type = msu_type_by_id(type_id);
    msu->data_p->data = create_action->creation_init_data;

    // TODO: queue_ws, msu_ws, what????

    // TODO: What's all that stuff about global pointers in the old code ???

    if (msu->type == NULL){
        /* for enqueing failure messages*/
        log_error("Unknown MSU type %d", type_id);
        msu_failure(msu_id);
        msu_free(msu);
        return NULL;
    }

    // Call the type-specific initialization function
    // TODO: What is this "creation_init_data"
    if (msu->type->init){
        int rtn = msu->type->init(msu, create_action);
        if (rtn){
            log_error("MSU creation failed for %s MSU: id %d",
                      msu->type->name, msu_id);
            msu_failure(msu_id);
            msu_free(msu);
            return NULL;
        }
    }
    log_info("Created %s MSU: id %d", msu->type->name, msu_id);
    msu->rt_table = NULL;
    msu->scheduling_weight = 1;
    msu->routing_state = NULL;
    return msu;
}

/** Frees an instance of an MSU along with associated structures.
 * Calls type-specific destructor if applicable.
 * NOTE: Does **NOT** free msu->data -- that must be freed manually
 *       with msu_data_free()
 * @param msu The MSU to be freed
 */
void destroy_msu(struct generic_msu *msu)
{
    if (msu->type->destroy)
        msu->type->destroy(msu);
    msu_free(msu);
}

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
int default_deserialize(struct generic_msu *self, intermsu_msg *msg,
                        void *buf, uint16_t bufsize){
    if (self){
        msu_queue_item *recvd =  malloc(sizeof(*recvd));
        if (!(recvd))
            return -1;
        recvd->buffer_len = bufsize;
        recvd->buffer = malloc(bufsize);
        if (!(recvd->buffer)){
            free(recvd);
            return -1;
        }
        memcpy(recvd->buffer, msg->payload, bufsize);
        generic_msu_queue_enqueue(&self->q_in, recvd);
        return 0;
    }
    return -1;
}

/** Serializes the data of an msu_queue_item and sends it
 * to be deserialized by a remote msu.
 *
 * Copies data->buffer onto msg->payload. If something more
 * complicated has to be done, use a type-specific implementation
 * of this function
 *
 * Frees the message after enqueuing
 *
 * @param src MSU sending the message
 * @param data item to be enqueued onto next MSU
 * @param dst MSU destination to receive the message
 * @return -1 on error, >=0 on success
 */
int default_send_remote(struct generic_msu *src, msu_queue_item *data,
                        struct msu_endpoint *dst){
    struct dedos_intermsu_message *msg = malloc(sizeof(*msg));
    if (!msg){
        log_error("Unable to allocate memory for intermsu msg%s", "");
        return -1;
    }

    msg->dst_msu_id = dst->id;
    msg->src_msu_id = src->id;

    // TODO: Is this next line right? src->proto_number?
    msg->proto_msg_type = src->type->proto_number;
    msg->payload_len = data->buffer_len;
    msg->payload = malloc(msg->payload_len);
    if (!msg->payload){
        log_error("Unable to allocate memory for intermsu payload %s", "");
        free(msg);
        return -1;
    }

    memcpy(msg->payload, data->buffer, msg->payload_len);

    struct dedos_thread_msg *thread_msg = malloc(sizeof(*thread_msg));
    if (!thread_msg){
        log_error("Unable to allocate dedos_thread_msg%s", "");
        free(msg->payload);
        free(msg);
        return -1;
    }
    thread_msg->next = NULL;
    thread_msg->action = FORWARD_DATA;
    thread_msg->action_data = dst->ipv4;

    thread_msg->buffer_len = sizeof(*msg) + msg->payload_len;
    thread_msg->data = msg;

    /* add to allthreads[0] queue,since main thread is always at index 0 */
    /* need to create thread_msg struct with action = forward */

    int rtn = dedos_thread_enqueue(main_thread->thread_q, thread_msg);
    if (rtn < 0){
        log_error("Failed to enqueue data in main thread queue%s", "");
        free(thread_msg);
        free(msg->payload);
        free(msg);
        return -1;
    }
    log_debug("Successfully enqueued msg in main queue, size: %d", rtn);
    return rtn;
}

/**
 * Default function to enqueue data onto a local msu.
 *
 * I see no reason why this function wouldn't suffice for all msus,
 * but if more (or more complicated) data structures must be sent,
 * provide a custom implementation in the msu_type source.
 *
 * @param src MSU sending the data
 * @param data queue item to be sent
 * @param dst MSU receiving the data
 * @return 0 on success, -1 on error
 */
int default_send_local(struct generic_msu *src, msu_queue_item *data,
                       struct msu_endpoint *dst){
    log_debug("Enqueuing locally to dst with id%d", dst->id);
    return generic_msu_queue_enqueue(dst->next_msu_input_queue, data);
}

struct round_robin_hh_key{
    int type_id;
    uint32_t ip_address;
};

/**
 * Simple structure to store an MSUs routing state when using
 * the round-robin routing method.
 * Will soon be replaced by routing object and hash-based method
 */
struct round_robin_state {
    struct round_robin_hh_key id; /**< ID of the type stored in this element in the struct */
    struct msu_endpoint *last_endpoint; /**< Last endpoint of that type that was sent to */
    UT_hash_handle hh; /**< Hash-handle */
};


/** Routing function to deliver traffic to a set of MSUs.
 * TODO: This will soon be moved to a "router" object, instead of
 * being handled by the destination type.
 *
 * @param type msu_type to be delivered to
 * @param sender msu sending the data
 * @param data queue item to be delivered
 * @return msu to which the message is to be enqueued, or NULL if no MSU could be found
 */
struct msu_endpoint *round_robin(struct msu_type *type, struct generic_msu *sender,
                                 msu_queue_item *data){
    struct msu_endpoint *dst_msus =
        get_all_type_msus(sender->rt_table, type->type_id);

    if (! (dst_msus) ){
        log_error("Source MSU %d did not find target MSU of type %d",
                  sender->id, type->type_id);
        return NULL;
    }

    struct round_robin_hh_key key = {type->type_id, 0};

    struct round_robin_state *all_states =
            (struct round_robin_state *)sender->routing_state;
    struct round_robin_state *route_state = NULL;
    HASH_FIND(hh, all_states, &key, sizeof(key), route_state);

    // Routing hasn't been initialized yet, so choose a semi-random
    // starting point
    if (route_state == NULL){
        log_debug("Creating new routing state for type %d from msu %d",
                  type->type_id, sender->id);
        struct msu_endpoint *last_endpoint = dst_msus;
        // TODO: Need to free when freeing msu!
        route_state = malloc(sizeof(*route_state));
        for (int i=0; i< (sender->id) % HASH_COUNT(dst_msus); i++){
            last_endpoint = last_endpoint->hh.next;
            if (! last_endpoint)
                last_endpoint = dst_msus;
        }
        route_state->id = key;
        route_state->last_endpoint = last_endpoint;
        HASH_ADD(hh, all_states, id, sizeof(key), route_state);
        sender->routing_state = (void*)all_states;
    }

    // Get the next endpoint of the same type, and replace the current one with it
    route_state->last_endpoint = route_state->last_endpoint->hh.next;
    // If it's null, just get the first endpoint
    if (! route_state->last_endpoint)
       route_state->last_endpoint = dst_msus;
    return route_state->last_endpoint;
}

/** Calls typical round_robin routing iteratively until it gets a destination
 * with the same ip address as specified.
 *
 * NOTE: As the queue item would contain ip addresses in a msu-type specific format,
 *       ip_address must be extracted by the destination msu before being passed into
 *       this function. Thus, code should look something like:
 *       ...
 *       type->route = type_do_routing;
 *       ...
 *       struct msu_endpoint type_do_routing(msu_type *type, struct generic_msu *sender,
 *                                           msu_queue_item *data){
 *           uint32_t ip_address = extract_ip_from_type_data(data);
 *           return round_robin_within_ip(type, sender, ip_address);
 *       }
 *
 * TODO: Soon to be handled by router object.
 *
 * @param type MSU type to be located
 * @param sender MSU sending the data
 * @param ip_address requested ip of destination MSU
 * @return msu_endpoint with requested IP, or NULL if N/A
 */
struct msu_endpoint *round_robin_within_ip(struct msu_type *type, struct generic_msu *sender,
                                           uint32_t ip_address){
    struct msu_endpoint *dst_msus =
        get_all_type_msus(sender->rt_table, type->type_id);

    if (! (dst_msus) ){
        log_error("Source MSU %d did not find target MSU of type %d",
                  sender->id, type->type_id);
        return NULL;
    }

    struct round_robin_hh_key key = {type->type_id, ip_address};

    struct round_robin_state *all_states =
            (struct round_robin_state *)sender->routing_state;
    struct round_robin_state *route_state = NULL;
    HASH_FIND(hh, all_states, &key, sizeof(key), route_state);

    // Routing hasn't been initialized yet, so choose a semi-random
    // starting point
    if (route_state == NULL){
        log_debug("Creating new routing state for type %d from msu %d",
                  type->type_id, sender->id);
        struct msu_endpoint *last_endpoint = dst_msus;
        // TODO: Need to free when freeing msu!
        route_state = malloc(sizeof(*route_state));
        for (int i=0; i< (sender->id) % HASH_COUNT(dst_msus); i++){
            last_endpoint = last_endpoint->hh.next;
            if (! last_endpoint)
                last_endpoint = dst_msus;
        }
        route_state->id = key;
        route_state->last_endpoint = last_endpoint;
        HASH_ADD(hh, all_states, id, sizeof(key), route_state);
        sender->routing_state = (void*)all_states;
    }

    // Now loop over until you find an MSU with the right IP
    for (int i=0; i<HASH_COUNT(dst_msus); i++){
        // Get the next endpoint of the same type, and replace the current one with it
        route_state->last_endpoint = route_state->last_endpoint->hh.next;
        // If it's null, just get the first endpoint
        if (! route_state->last_endpoint)
           route_state->last_endpoint = dst_msus;

        if (route_state->last_endpoint->ipv4 == ip_address)
            break;
    }

    if ( route_state->last_endpoint->ipv4 != ip_address){
        log_error("Could not find destination of type %d with correct ip from sender %d",
                type->type_id, sender->id);
        return NULL;
    }
    return route_state->last_endpoint;
}

/**
 * Private function, sends data stored in a queue_item to a destination msu,
 * either local or remote.
 *
 * @param dst Destination MSU
 * @param src Source MSU
 * @param data Data to be enqueued on or sent to destination
 * @return -1 on error, 0 on success
 */
int send_to_dst(struct msu_endpoint *dst, struct generic_msu *src, msu_queue_item *data){
    if (dst->locality == MSU_LOC_SAME_RUNTIME){
        if (!(dst->next_msu_input_queue)){
            log_error("Queue pointer not found%s", "");
            return -1;
        }
        int rtn = src->type->send_local(src, data, dst);
        if (rtn < 0){
            log_error("Failed to enqueue next msu%s", "");
            return -1;
        }
        return 0;
    } else if (dst->locality == MSU_LOC_REMOTE_RUNTIME){
        int rtn = src->type->send_remote(src, data, dst);
        if (rtn < 0){
            log_error("Failed to send to remote runtime%s", "");
        }
        if (data){
            log_debug("Freeing data buffer and data because of remote send");
            free(data->buffer);
            free(data);
        }
        return rtn;
    } else {
        log_error("Unknown locality: %d", dst->locality);
        return -1;
    }
}

/** Receives and handles dequeued data from another MSU.
 * Also handles sending message to the next MSU, if applicable.
 * Calls the receieve function of the provided MSU type if available.
 * @param self MSU receiving data
 * @param data received data
 * @return 0 on success, -1 on error
 */
int msu_receive(struct generic_msu *self, msu_queue_item *data){

    // Check that all of the relevant structures exist
    if (! (self && self->type->receive) ){
        if (data){
            free(data->buffer);
            free(data);
        }
        log_error("Could not receive data%s", "");
        return -1;
    }

    // Receieve the data, get the type of the next msu to send to
    aggregate_start_time(MSU_INTERNAL_TIME, self->id);
    int type_id = self->type->receive(self, data);
    aggregate_end_time(MSU_INTERNAL_TIME, self->id);
    if (type_id <= 0){
        // If type_id <= 0 it can either be an error (< 0) or just
        // not have a subsequent destination
        if (type_id < 0){
            ;//log_warn("MSU %d returned error code %d", self->id, type_id);
        }
        if (data){
            free(data->buffer);
            free(data);
        }
        return type_id;
    }
    log_debug("type ID of next MSU is %d", type_id);

    // Get the MSU type to deliver to
    struct msu_type *type = msu_type_by_id((unsigned int)type_id);
    if (type == NULL){
        log_error("Type ID %d not recognized", type_id);
        if (data){
            free(data->buffer);
            free(data);
        }
        return -1;
    }

    // Get the specific MSU to deliver to
    struct msu_endpoint *dst = type->route(type, self, data);
    if (dst == NULL){
        log_error("No destination endpoint of type %s (%d) for msu %d",
                  type->name, type_id, self->id);
        if (data){
            free(data->buffer);
            free(data);
        }
        return -1;
    }
    log_debug("Next msu id is %d", dst->id);

    // Send to the specific destination
    int rtn = send_to_dst(dst, self, data);
    if (rtn < 0){
        log_error("Error sending to destination msu %s", "");
        if (data){
            free(data->buffer);
            free(data);
        }
    }

    return rtn;
}
