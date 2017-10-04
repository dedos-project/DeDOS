/**
 * @file msu_calls.c
 * Defines methods used for calling MSUs from other MSUs
 */

#include "msu_calls.h"
#include "logging.h"
#include "thread_message.h"
#include "output_thread.h"
#include "routing_strategies.h"
#include "profiler.h"

/**
 * Calls type-specific MSU serialization function and enqueues data into main
 * thread queue so it can be forwarded to remote MSU
 * @param msg Message to be enqueued to the remote MSU
 * @param dst_type MSU type of the destination
 * @param dst The specific MSU destination
 * @return 0 on success, -1 on error
 */
static int enqueue_for_remote_send(struct msu_msg *msg,
                                   struct msu_type *dst_type,
                                   struct msu_endpoint *dst) {
    size_t size;
    // Is it okay that I malloc within the serialize fn but have to free here on error?
    void *serialized = serialize_msu_msg(msg, dst_type, &size);
    if (serialized == NULL) {
        log_error("Error serializing MSU msg for delivery to msu %d", dst->id);
        return -1;
    }

    struct thread_msg *thread_msg = init_send_thread_msg(dst->runtime_id,
                                                         dst->id,
                                                         size,
                                                         serialized);

    if (thread_msg == NULL) {
        log_error("Error creating thread message for sending MSU msg to msu %d",
                  dst->id);
        free(serialized);
        return -1;
    }

    PROFILE_EVENT(msg->hdr, PROF_REMOTE_SEND);
    int rtn = enqueue_for_output(thread_msg);
    if (rtn < 0) {
        log_error("Error enqueuing MSU msg for msu %u on main thread", dst->id);
        destroy_thread_msg(thread_msg);
        free(serialized);
        return -1;
    }

    log(LOG_MSU_ENQUEUES, "Enqueued message for remote send to dst %d on runtime %d",
               dst->id, dst->runtime_id);
    return 0;
}

/** Enqueues a message in the queue of a local MSU.
 * TESTME: call_local_msu()
 *
 * @param sender The MSU sending the message
 * @param dest The MSU to receive the message
 * @param hdr The header of the message received by the sender. To be modified
 *            with provinance and used in routing decisions
 * @param data_size Size of the data to be enqueued to the next MSU
 * @param data The data to be enqueued to the next MSU
 * @return 0 on success, -1 on error
 */
int call_local_msu(struct local_msu *sender, struct local_msu *dest,
                struct msu_msg_hdr *hdr, size_t data_size, void *data) {
    struct msu_msg *msg = create_msu_msg(hdr, data_size, data);
    log(LOG_MSU_ENQUEUES, "Enqueing data %p directly to destination %d",
               msg->data, dest->id);

    int rtn = add_provinance(&msg->hdr.provinance, sender);
    if (rtn < 0) {
        log_warn("Could not add provinance to message %p", msg);
    }

    rtn = enqueue_msu_msg(&dest->queue, msg);
    if (rtn < 0) {
        log_error("Error enqueing data %p to local MSU %d", msg->data, dest->id);
        free(msg);
        return -1;
    }
    return 0;
}

/** Enqueues a new message in the queue of a local MSU.
 * This function is identical to `call_local_msu()` with the exception that it
 * creates the required header from the specified message key. It is to be used
 * for initial enqueueing rather than existing message passsing
 *
 * @param sender The MSU sending the message
 * @param dest The MSU to receive the message
 * @param key The key to assign to the header of the new MSU message
 * @param data_size Size of the data to be enqueued to the next MSU
 * @param data the data to be enqueued to the next MSU
 * @return 0 on success, -1 on error
 */
int init_call_local_msu(struct local_msu *sender, struct local_msu *dest,
                        struct msu_msg_key *key, size_t data_size, void *data) {
    struct msu_msg_hdr hdr;
    if (init_msu_msg_hdr(&hdr, key) != 0) {
        log_error("Error initializing message header");
        return -1;
    }
    SET_PROFILING(hdr);
    PROFILE_EVENT(hdr, PROF_DEDOS_ENTRY);
    return call_local_msu(sender, dest, &hdr, data_size, data);
}



/**
 * Sends an MSU message to a destination of the given type,
 * utilizing the sending MSU's routing function.
 *
 * @param sender The MSU sending the message
 * @param dst_type The MSU type to receive the message
 * @param hdr The header to be used in routing, passed on from the sender's received message
 * @param data_size Size of the data to be enqueued
 * @param data The data to be enqueued
 * @return 0 on success, -1 on error
 */
int call_msu_type(struct local_msu *sender, struct msu_type *dst_type,
                  struct msu_msg_hdr *hdr, size_t data_size, void *data) {

    struct msu_msg *msg = create_msu_msg(hdr, data_size, data);

    int rtn = add_provinance(&msg->hdr.provinance, sender);
    if (rtn < 0) {
        log_warn("Could not add provinance to message %p", msg);
    }

    log(LOG_MSU_ENQUEUES, "Sending data %p to destination type %s",
               msg->data, dst_type->name);

    struct msu_endpoint dst;
    if (dst_type->route) {
        rtn = dst_type->route(dst_type, sender, msg, &dst);
    } else {
        rtn = default_routing(dst_type, sender, msg, &dst);
    }

    if (rtn < 0) {
        log_error("Could not find destination endpoint of type %s from msu %d (%s). "
                  "Dropping message %p",
                  dst_type->name, sender->id, sender->type->name, msg);
        free(msg);
        return -1;
    }


    switch (dst.locality) {
        case MSU_IS_LOCAL:
            rtn = enqueue_msu_msg(dst.queue, msg);
            if (rtn < 0) {
                log_error("Error enqueuing data %p to local MSU %d", msg->data, dst.id);
                free(msg);
                return -1;
            }
            log(LOG_MSU_ENQUEUES, "Enqueued data %p to local msu %d", msg->data, dst.id);
            return 0;
        case MSU_IS_REMOTE:
            rtn = enqueue_for_remote_send(msg, dst_type, &dst);
            if (rtn < 0) {
                log_error("Error sending data %p to remote MSU %d", msg->data, dst.id);
                return -1.i;
            }
            log(LOG_MSU_ENQUEUES, "Sending data %p to remote msu %d", msg->data, dst.id);

            // Since the data has been sent to a remote MSU, we can now
            // free the msu message from this runtime's memory
            destroy_msu_msg_and_contents(msg);
            return 0;
        default:
            log_error("Unknown MSU locality for msu %d (from %d): %d",
                      dst.id, sender->id, dst.locality);
            return -1;
    }
}

/**
 * Sends an MSU message to a destination of the specified type.
 * This function is identical to `call_msu_type()` with the exception that it
 * performs header initialization. To be used when performing an initial enqueue, rather
 * than continuing a chain of enqueues.
 * @param sender The MSU sending the message
 * @param dst_type The MSU type to receive the message
 * @param key The key to assign to the message header
 * @param data_size The size of the data to be enqueued
 * @param data The data to be enqueued
 * @return 0 on success, -1 on error
 */
int init_call_msu_type(struct local_msu *sender, struct msu_type *dst_type,
                       struct msu_msg_key *key, size_t data_size, void *data) {
    struct msu_msg_hdr hdr;
    if (init_msu_msg_hdr(&hdr, key) != 0) {
        log_error("Error initializing message header");
        return -1;
    }
    SET_PROFILING(hdr);
    PROFILE_EVENT(hdr, PROF_DEDOS_ENTRY);
    return call_msu_type(sender, dst_type, &hdr, data_size, data);
}

/**
 * Sends an MSU message to a speicific destination, either local or remote.
 *
 * @param sender The MSU sending the message
 * @param endpoint The endpoint to which the message is to be delivered
 * @param endpoint_type The MSU type of the endpoint to receive the message
 * @param hdr The header of the MSU message received by the sender
 * @param data_size The size of the sending data
 * @param data The data sent
 * @return 0 on success, -1 on error
 */
int call_msu_endpoint(struct local_msu *sender, struct msu_endpoint *endpoint,
                      struct msu_type *endpoint_type, 
                      struct msu_msg_hdr *hdr, size_t data_size, void *data) {
    struct msu_msg *msg = create_msu_msg(hdr, data_size, data);

    int rtn = add_provinance(&msg->hdr.provinance, sender);
    if (rtn < 0) {
        log_warn("Could not add provinance to message %p", msg);
    }

    log(LOG_MSU_ENQUEUES, "Sending data %p to destination endpoint %d",
        msg->data, endpoint->id);

    switch (endpoint->locality) {
        case MSU_IS_LOCAL:
            rtn = enqueue_msu_msg(endpoint->queue, msg);
            if (rtn < 0) {
                log_error("Error enqueuing data %p to local msu %d", msg->data, endpoint->id);
                free(msg);
                return -1;
            }
            log(LOG_MSU_ENQUEUES, "Enqueued data %p to local msu %d", msg->data, endpoint->id);
            return 0;
        case MSU_IS_REMOTE:
            rtn = enqueue_for_remote_send(msg, endpoint_type, endpoint);
            if (rtn < 0) {
                log_error("Error enqueueing data %p towards remote msu %d", 
                          msg->data, endpoint->id);
                free(msg);
                return -1;
            }
            log(LOG_MSU_ENQUEUES, "Enqueued data %p toward remote msu %d",
                 msg->data, endpoint->id);
            return 0;
        default:
            log_error("Unknown MSU locality: %d", endpoint->locality);
            return -1;
    }
}

/**
 * Sends an MSU message to a specific destination, either local or remote.
 *
 * This function is identical to `call_msu_endpoint()` with the exception that
 * it constructs the header for the message and is to be used for initial enqueues.
 * @param sender The MSU sending the message
 * @param endpoint The endpoint to which the message is to be delivered
 * @param endpoint_type The MSU type of the endpoint to receive the message
 * @param hdr The header of the MSU message received by the sender
 * @param data_size The size of the sending data
 * @param data The data sent
 * @return 0 on success, -1 on error
 */
int init_call_msu_endpoint(struct local_msu *sender, struct msu_endpoint *endpoint,
                           struct msu_type *endpoint_type,
                           struct msu_msg_key *key, size_t data_size, void *data) {
    struct msu_msg_hdr hdr;
    if (init_msu_msg_hdr(&hdr, key) != 0) {
        log_error("Error initializing message header");
        return -1;
    }
    SET_PROFILING(hdr);
    PROFILE_EVENT(hdr, PROF_DEDOS_ENTRY);
    return call_msu_endpoint(sender, endpoint, endpoint_type, &hdr, data_size, data);
}
