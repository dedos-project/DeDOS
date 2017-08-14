struct create_worker_thread_msg {
    int thread_id;
    enum thread_behavior blocking;
};

struct worker_thread {
    struct dedos_thread *thread;
    int n_msus;
    struct local_msu *msus[MAX_MSU_PER_THREAD];
};

void init_worker_thread(struct worker_thread *worker, struct dedos_thread *thread) {
    worker->thread = thread;
    worker->n_msu = 0;
}

#define CHECK_MSG_SIZE(msg, target) \
    if (msg->data_size != sizeof(target)) { \
        log_warn("Message data size does not match size" \
                 "of target type " #target ); \
        break; \
    }

int process_worker_thread_msg(struct thread_msg *msg) {
    int rtn;
    switch (msg->type) {
        case CREATE_MSU: 
            
            

int worker_thread_loop(struct dedos_thread *thread, struct dedos_thread *main_thread) {
    log_info("Starting worker thread loop %d", self->id);

    struct worker_thread self;
    init_worker_thread(&self, thread);
    // TODO: Get context switches

    while (1) {
        int rtn = thread_wait(thread);
        for (int i=0; i<self->n_msus; i++) {
            struct msu_msg *msg = dequeue_msu_msg(msus[i]->q);
            if (msg){
                msu_receive(msus[i], msg);
                free(msg);
            }
            msg = dequeue_msu_msg(msus[i]->q_control);
            if (msg) {
                msu_receive_ctrl(msus[i], msg);
                free(msg);
            }
        }
        struct thread_msg *msg = dequeue_thread_msg(thread);
        while (msg != NULL) {
            int rtn = process_worker_thread_msg(msg);
            free(msg);
            msg = dequeue_thread_msg(thread);
        }
    }
    log_info("Leaving thread %d", thread->id);
    return 0;
}


struct worker_thread *create_worker_thread(struct create_worker_thread_msg *msg, 
                                           struct dedos_thread *main_thread) {
    struct worker_thread *worker_thread = malloc(sizeof(*worker));
    int rtn = start_dedos_thread(worker_thread_loop, msg->blocking, 
                                 msg->thread_id, thread, main_thread);
    if (rtn < 0) {
        log_error("Error starting dedos thread %d", msg->thread_id);
        return NULL;
    }
    log_info("Created worker thread %d", msg->thread_id);
    return worker_thread;
}
        
