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

int schedule_local_msu_call(struct local_msu *sender, struct local_msu *dest, struct timespec *interval,
                            struct msu_msg_hdr *hdr, size_t data_size, void *data) {
    struct msu_msg *msg = create_msu_msg(hdr, data_size, data);
    log(LOG_MSU_ENQUEUES, "Enqueing data %p directly to destination %d",
               msg->data, dest->id);

    int rtn = add_provinance(&msg->hdr.provinance, sender);
    if (rtn < 0) {
        log_warn("Could not add provinance to message %p", msg);
    }

    rtn = schedule_msu_msg(&dest->queue, msg, interval);
    if (rtn < 0) {
        log_error("Error enqueing data %p to local MSU %d", msg->data, dest->id);
        free(msg);
        return -1;
    }
    rtn = enqueue_worker_timeout(dest->thread, interval);
    if (rtn < 0) {
        log_warn("Error enqueing timeout to worker thread");
    }
    return 0;
}

int schedule_local_msu_init_call(struct local_msu *sender, struct local_msu *dest, struct timespec *interval,
                      struct msu_msg_key *key, size_t data_size, void *data) {
    struct msu_msg_hdr hdr;
    if (init_msu_msg_hdr(&hdr, key) != 0) {
        log_error("Error initializing message header");
        return -1;
    }
    SET_PROFILING(hdr);
    PROFILE_EVENT(hdr, PROF_DEDOS_ENTRY);
    return schedule_local_msu_call(sender, dest, interval, &hdr, data_size, data);
}


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

    msg->hdr.key.group_id = dst.route_id;

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
