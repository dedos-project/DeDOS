#include "local_msu.h"
#include "routing_strategies.h"
#include "logging.h"
#include "rt_stats.h"
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

static enum stat_id MSU_STAT_IDS[] = {
    MSU_QUEUE_LEN,
    MSU_ITEMS_PROCESSED,
    MSU_EXEC_TIME,
    MSU_IDLE_TIME,
    MSU_MEM_ALLOC,
    MSU_NUM_STATES
};

static int NUM_MSU_STAT_IDS = sizeof(MSU_STAT_IDS) / sizeof(enum stat_id);

/**
 * Initializes the stat IDS that are relevant to MSUs
 */
static void init_msu_stats(int msu_id) {
    for (int i=0; i<NUM_MSU_STAT_IDS; i++) {
        if (init_stat_item(MSU_STAT_IDS[i], msu_id) != 0) {
            log_warn("Could not initialize stat item %d for msu %d", MSU_STAT_IDS[i], msu_id);
        }
    }
}


/**
 * Allocates and creates a new MSU of the specified type and ID on the given thread
 */
struct local_msu *init_msu(unsigned int id,
                           struct msu_type *type,
                           struct worker_thread *thread,
                           struct msu_init_data *data) {
    struct local_msu *msu = msu_alloc();
    init_msu_stats(id);
    msu->thread = thread;
    msu->id = id;
    msu->type = type;
    msu->scheduling_weight = 0;

    if (init_msg_queue(&msu->queue, &thread->thread->sem) != 0) {
        msu_free(msu);
        log_error("Error initializing msu queue");
        return NULL;
    }

    // Must be done before running init function
    int rtn = register_msu_with_thread(msu);
    if (rtn < 0) {
        log_error("Error registering MSU With thread");
        msu_free(msu);
        rm_from_local_registry(msu->id);
        return NULL;
    }
    // TODO: Unregister if creation fails

    log_info("Initializing msu (ID: %d, type: %s, data: '%s')", id, type->name,
             data->init_data);
    if (type->init) {
        if (type->init(msu, data) != 0) {
            log_error("Error running MSU %d (type: %s) type-specific initialization function",
                      id, type->name);
            msu_free(msu);
            return NULL;
        }
    }
    rtn = add_to_local_registry(msu);
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
static int msu_receive(struct local_msu *msu, struct msu_msg *msg) {

    record_end_time(MSU_IDLE_TIME, msu->id);
    record_start_time(MSU_EXEC_TIME, msu->id);
    int rtn = msu->type->receive(msu, msg);
    record_end_time(MSU_EXEC_TIME, msu->id);
    record_start_time(MSU_IDLE_TIME, msu->id);

    if (rtn != 0) {
        log_error("Error executing MSU %d (%s) receive function",
                  msu->id, msu->type->name);
        return -1;
    }
    return 0;
}

int msu_dequeue(struct local_msu *msu) {
    struct msu_msg *msg = dequeue_msu_msg(&msu->queue);
    if (msg) {
        log(LOG_MSU_DEQUEUES, "Dequeued MSU message %p for msu %d", msg, msu->id);
        record_stat(MSU_QUEUE_LEN, msu->id, msu->queue.num_msgs, false);
        int rtn = msu_receive(msu, msg);
        increment_stat(MSU_ITEMS_PROCESSED, msu->id, 1);
        free(msg);
        return rtn;
    }
    return 1;
}

