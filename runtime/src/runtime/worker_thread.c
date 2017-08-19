#include "msu_type.h"

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

static int create_msu_on_thread(struct worker_thread *thread, struct create_msu_msg *msg) {
    struct msu_type *type = get_msu_type(msg->msu_type_id);
    if (type == NULL) {
        log_error("Failed to create MSU %d. Cannot retrieve type", msg->msu_id);
        return -1;
    }
    struct local_msu *msu = init_msu(msg->msu_id, type, thread, &msg->init_data);
    if (msu == NULL) {
        log_error("Error creating MSU %d. Not placing on thread %d",
                  msg->msu_id, thread->thread->id);
        return -1;
    }
    thread->msus[thread->n_msus] = msu;
    thread->n_msus++;
    return 0;
}

static int worker_add_msu_route(struct msu_route_msg *msg) {
}

static int worker_del_msu_route(struct msu_route_msg *msg) {
}

#define CHECK_MSG_SIZE(msg, target) \
    if (msg->data_size != sizeof(target)) { \
        log_warn("Message data size does not match size" \
                 "of target type " #target ); \
        break; \
    }

int process_worker_thread_msg(struct worker_thread *thread, struct thread_msg *msg) {
    int rtn;
    switch (msg->type) {
        case CREATE_MSU:
            CHECK_MSG_SIZE(msg, struct create_msu_msg);
            struct create_msu_msg *create_msg = msg->data;
            rtn = create_msu_on_thread(thread, create_msg);
            if (rtn < 0) {
                log_error("Error creating MSU");
            }
            break;
        case DELETE_MSU:
            CHECK_MSG_SIZE(msg, struct delete_msu_msg);
            struct delete_msu_msg *del_msg = msg->data;
            rtn = delete_msu_from_thread(thread, del_msg);
            if (rtn < 0) {
                log_error("Error deleting MSU");
            }
            break;
        case ADD_ROUTE:
        case DEL_ROUTE:
            CHECK_MSG_SIZE(msg, struct msu_route_msg);
            struct msu_route_msg *route_msg = msg->data;
            if (msg->type == ADD_ROUTE) {
                rtn = worker_add_msu_route(route_msg);
            } else {
                rtn = worker_del_msu_route(route_msg);
            }
            if (rtn < 0) {
                log_error("Error modifiying MSU route");
            }
            break;
        case ADD_RUNTIME:
        case CREATE_THREAD:
        case SEND_TO_RUNTIME:
        case MODIFY_ROUTE:
            log_error("Message (type %d) meant for main thread send to worker thread",
                      msg->type);
            break;
        default:
            log_error("Unknown message type %d delivered to worker thread %d",
                      msg->type, thread->thread->id);
            break;
    }



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
            int rtn = process_worker_thread_msg(thread, msg);
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

