
/**
 * Allocates and creates a new MSU of the specified type and ID on the given thread
 */
struct local_msu *init_msu(unsigned int id, struct msu_type *type,
                           struct worker_thread *thread, struct msu_init_data *data) {
    struct local_msu *msu = msu_alloc();
    msu->id = msu_id;
    msu->type = type;
    msu->scheduling_weight = 0;

    if (init_msg_queue(&msu->msu_queue) != 0) {
        msu_free(msu);
        log_error("Error initializing msu queue");
        return NULL;
    }
    
    if (init_msg_queue(&msu->ctrl_queue) != 0) {
        msu_free(msu);
        log_error("Error initializing control queue");
        return NULL;
    }

    msu->msu_state = NULL;

    if (msu->type->init != NULL) {
        if (msu->type->init(msu, data) != 0) {
            log_error("Error running MSU %d (type: %s) type-specific initialization function",
                      id, type->name);
            msu_free(msu);
            return NULL;
        }
    }
    return NULL;
}
    
