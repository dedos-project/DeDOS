#include "local_msu.h"
#include "logging.h"
#include "stats.h"
#include "msu_message.h"
#include "inter_runtime_messages.h"
#include "main_thread.h"
#include "thread_message.h"
#include <stdlib.h>

#define MAX_MSU_ID 1024

// TODO: This lock might not be useful, as I believe this registry will
// TODO: only ever be accessed from the socket handler thread
pthread_rwlock_t msu_registry_lock;
struct local_msu *local_msu_registry[MAX_MSU_ID];

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

static int rm_from_local_registry(int id) {
    if (pthread_rwlock_wrlock(&msu_registry_lock) != 0) {
        log_perror("Error opening write lock on msu registry");
        return -1;
    }
    int rtn = 0;
    if (local_msu_registry[id] == NULL) {
        log_warn("MSU with id %d does not exist and cannot be deleted", id);
        rtn = -1;
    } else {
        local_msu_registry[id] = NULL;
    }
    if (pthread_rwlock_unlock(&msu_registry_lock) != 0) {
        log_perror("Error unlocking msu registry");
        return -1;
    }
    return rtn;
}

static int add_to_local_registry(struct local_msu *msu) {
    if (pthread_rwlock_wrlock(&msu_registry_lock) != 0) {
        log_perror("Error opening write lock on msu registry");
        return -1;
    }
    int rtn = 0;
    if (local_msu_registry[msu->id] != NULL) {
        log_error("MSU with id %d already exists and cannot be added to registry", msu->id);
        rtn = -1;
    } else {
        local_msu_registry[msu->id] = msu;
    }
    if (pthread_rwlock_unlock(&msu_registry_lock) != 0) {
        log_perror("Error unlocking msu registry");
        return -1;
    }
    return rtn;
}

struct local_msu *get_local_msu(unsigned int id) {
    if (pthread_rwlock_rdlock(&msu_registry_lock) != 0) {
        log_perror("Error opening read lock on MSU registry");
        return NULL;
    }
    struct local_msu *msu = local_msu_registry[id];
    if (pthread_rwlock_unlock(&msu_registry_lock) != 0) {
        log_perror("Error unlocking msu registry");
        return NULL;
    }
    if (msu == NULL) {
        log_error("Could not get local msu with id %d. Not registered.", id);
    }
    return msu;
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

    if (init_msg_queue(&msu->queue, &thread->thread->sem) != 0) {
        msu_free(msu);
        log_error("Error initializing msu queue");
        return NULL;
    }

    if (type->init) {
        if (type->init(msu, data) != 0) {
            log_error("Error running MSU %d (type: %s) type-specific initialization function",
                      id, type->name);
            msu_free(msu);
            return NULL;
        }
    }
    int rtn = add_to_local_registry(msu);
    if (rtn < 0) {
        log_error("Error adding MSU to local registry");
        msu_free(msu);
        return NULL;
    }
    return msu;
}

/**
 * Calls type-specific destroy function and frees associated memory
 * @param msu Msu to destroy
 */
void destroy_msu(struct local_msu *msu) {
    int id = msu->id;
    if (msu->type->destroy) {
        msu->type->destroy(msu);
    }
    msu_free(msu);
    rm_from_local_registry(id);
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

/**
 * Calls type-specific MSU serialization function and enqueues data into main
 * thread queue so it can be forwarded to remote MSU
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

    int rtn = enqueue_to_main_thread(thread_msg);
    if (rtn < 0) {
        log_error("Error enqueuing MSU msg for msu %u on main thread", dst->id);
        destroy_thread_msg(thread_msg);
        free(serialized);
        return -1;
    }

    log_custom(LOG_MSU_ENQUEUES, "Enqueued data %p for remote send to dst %d on runtime %d",
               data, dst->id, dst->runtime_id);
    return 0;
}

/**
 * Sends an MSU message to a destination of the given type,
 * utilizing the sending MSU's routing table.
 * Note: data_p is a double-pointer because in the case of a successful remote send,
 * *data_p is freed and set to NULL
 */
int send_to_msu(struct local_msu *sender, struct msu_type *dst_type, struct msu_msg **data_p) {
    struct msu_msg *data = *data_p;

    log_custom(LOG_MSU_ENQUEUES, "Sending data %p to destination type %s",
               data, type->name);

    struct msu_endpoint *dst = dst_type->route(dst_type, sender, data);
    if (dst == NULL) {
        log_error("Could not find destination endpoint of type %s from msu %d (%s). "
                  "Dropping message %p",
                  dst_type->name, sender->id, sender->type->name, data);
        return -1;
    }

    int rtn;
    switch (dst->locality) {
        case MSU_IS_LOCAL:
            rtn = enqueue_msu_msg(dst->queue, data);
            if (rtn < 0) {
                log_error("Error enqueuing data %p to local MSU %d", data, dst->id);
                return -1;
            }
            log_custom(LOG_MSU_ENQUEUES, "Enqueued data %p to local msu %d", data, dst->id);
            return 0;
        case MSU_IS_REMOTE:
            rtn = enqueue_for_remote_send(data, dst_type, dst);
            if (rtn < 0) {
                log_error("Error sending data %p to remote MSU %d", data, dst->id);
                return -1;
            }
            log_custom(LOG_MSU_ENQUEUES, "Sending data %p to remote msu %d", data, dst->id);

            // Since the data has been sent to a remote MSU, we can now
            // free the msu message from this runtime's memory
            destroy_msu_msg(data);
            *data_p = NULL;
            return 0;
        default:
            log_error("Unknown MSU locality: %d", dst->locality);
            return -1;
    }
}
