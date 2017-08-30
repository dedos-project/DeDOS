#include "local_msu.h"
#include "logging.h"
#include "stats.h"
#include "msu_message.h"
#include <stdlib.h>

/**
 * Allocates the memory associated with an MSU structure
 * @return Pointer to allocated MSU
 */
static struct local_msu *msu_alloc() {
    struct local_msu *msu = calloc(1, sizeof(*msu));
    if (msu == NULL) {
        log_error("Failed to allocate MSU");
        return NULL;
    }
    return msu;
}

/**
 * Frees the memory associated with an MSU structure
 */
static void msu_free(struct local_msu *msu) {
    free(msu);
}

/**
 * Allocates and creates a new MSU of the specified type and ID on the given thread
 */
struct local_msu *init_msu(unsigned int id,
                           struct msu_type *type,
                           struct worker_thread *thread,
                           struct msu_init_data *data) {
    struct local_msu *msu = msu_alloc();
    msu->id = id;
    msu->type = type;
    msu->scheduling_weight = 0;

    if (init_msg_queue(&msu->msu_queue) != 0) {
        msu_free(msu);
        log_error("Error initializing msu queue");
        return NULL;
    }

    if (type->init != NULL) {
        if (type->init(msu, data) != 0) {
            log_error("Error running MSU %d (type: %s) type-specific initialization function",
                      id, type->name);
            msu_free(msu);
            return NULL;
        }
    }
    return NULL;
}

/**
 * Calls type-specific destroy function and frees associated memory
 * @param msu Msu to destroy
 */
void destroy_msu(struct local_msu *msu) {
    if (msu->type->destroy) {
        msu->type->destroy(msu);
    }
    msu_free(msu);
}

/**
 * Calls type-specific MSU receive function and records execution time
 * @param msu MSU to receive data
 * @param data Data to be sent to MSU
 * @return 0 on success, -1 on error
 */
int msu_receive(struct local_msu *msu, struct msu_msg *data) {

    record_start_time(MSU_EXEC_TIME, msu->id);
    int rtn = msu->type->receive(msu, data);
    record_end_time(MSU_EXEC_TIME, msu->id);

    if (rtn != 0) {
        log_error("Error executing MSU %d (%s) receive function",
                  msu->id, msu->type->name);
        return -1;
    }
    return 0;
}

// TODO: enqueue_for_remote_send
int enqueue_for_remote_send(struct msu_msg *data, size_t size, unsigned int runtime_id) {return 0;}

/**
 * Calls type-specific MSU serialization function and enqueues data into main
 * thread queue so it can be forwarded to remote MSU
 */
static int send_to_remote_msu(struct msu_msg *data, struct msu_type *dst_type,
                       struct msu_endpoint *dst) {
    void *serialized = NULL;

    size_t size = dst_type->serialize(dst_type, data, &serialized);
    if (size <= 0) {
        log_error("Error serializing data %p for destination of type %s",
                  data, dst_type->name);
        return -1;
    }
    if (serialized == NULL) {
        log_error("Type %s serialization function did not set serialized buffer",
                  dst_type->name);
        return -1;
    }

    int rtn = enqueue_for_remote_send(data, size, dst->runtime_id);
    if (rtn < 0) {
        log_error("Error enqueing data %p for remote send to dst %d on runtime %d",
                  data, dst->id, dst->runtime_id);
        return -1;
    }
    log_custom(LOG_MSU_ENQUEUES, "Enqueued data %p for remote send to dst %d on runtime %d",
               data, dst->id, dst->runtime_id);
    return 0;
}

/**
 * Sends an MSU message to a destination of the given type, utilizing the sending MSU's
 * routing table
 */
int send_to_msu(struct local_msu *msu, struct msu_msg *data, struct msu_type *dst_type) {
    log_custom(LOG_MSU_ENQUEUES, "Sending data %p to destination type %s",
               data, type->name);

    struct msu_endpoint *dst = dst_type->route(dst_type, msu, data);
    if (dst == NULL) {
        log_error("Could not find destination endpoint of type %s from msu %d (%s). "
                  "Dropping message %p",
                  dst_type->name, msu->id, msu->type->name, data);
        return -1;
    }

    int rtn;
    switch (dst->locality) {
        case MSU_IS_LOCAL:;
            rtn = enqueue_msu_msg(dst->queue, data);
            if (rtn < 0) {
                log_error("Error enqueuing data %p to local MSU %d", data, dst->id);
                return -1;
            }
            log_custom(LOG_MSU_ENQUEUES, "Enqueued data %p to local msu %d", data, dst->id);
            return 0;
        case MSU_IS_REMOTE:
            rtn = send_to_remote_msu(data, dst_type, dst);
            if (rtn < 0) {
                log_error("Error sending data %p to remote MSU %d", data, dst->id);
                return -1;
            }
            log_custom(LOG_MSU_ENQUEUES, "Sending data %p to remote msu %d", data, dst->id);
            return 0;
        default:
            log_error("Unknown MSU locality: %d", dst->locality);
            return -1;
    }
}
