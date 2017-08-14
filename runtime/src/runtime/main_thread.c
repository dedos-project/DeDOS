
#ifndef LOG_RUNTIME_INIT
#define LOG_RUNTIME_INIT 0
#endif

#define MAIN_THREAD_ID 0

#define MAX_WORKER_THREADS 16
static struct worker_thread *worker_threads[MAX_WORKER_THREADS];
static int n_worker_threads = 0;

static int num_cpu = -1;

int set_num_cpu() {
    num_cpu = sysconf(_SC_NPROCESSORS_ONLN);
    log_custom(LOG_RUNTIME_INIT, "Number of cores on runtime: %d",
               num_cpu);
    return num_cpu;
}

/**
 * TODO: Actually get JSON from global controller?
 * ???: request_init_config
 */
int request_init_config() {
    struct dedos_control_msg msg = {
        .msg_type = REQUEST_MESSAGE,
        .msg_code = GET_INIT_CONFIG,
        // IMP: payload_len was num_cpu itself
        .payload_len = sizeof(num_cpu),
        .payload = (intptr_t)num_cpu;
    }
    return send_to_controller(&msg);
}

static int init_main_thread(struct dedos_thread *main_thread) {
    // TODO: Remove when main thread is just another thread
    if (init_dedos_thread(&main_thread, BLOCKING_THREAD, MAIN_THREAD_ID) != 0) {
        log_error("Error initializing main thread!");
        return -1;
    }

    if (init_msu_types() != 0) {
        log_warn("Error initializing MSU types");
    }

    if (init_runtime_epoll() != 0) {
        log_error("Error initializing runtime epoll");
        return -1;
    }

    struct sockaddr_in controller_addr;
    if (controller_address(&controller_addr) != 0) {
        log_error("Error getting controller address");
        return -1;
    }

    if (init_controller_socket(&controller_addr) != 0) {
        log_error("Error connecting to global controller");
        return -1;
    }

    int local_port = local_runtime_port();
    if (local_port < 0) {
        log_error("Error getting local runtime port");
        return -1;
    }

    if (init_runtime_sockets(local_port) != 0) {
        log_error("Error initializing runtime sockets");
        return -1;
    }

    return 0;
}

#define CHECK_MSG_SIZE(msg, target) \
    if (msg->data_size != sizeof(target)) { \
        log_warn("Message data size does not match size" \
                 "of target type " #target ); \
        break; \
    }

static int process_main_thread_msg(struct dedos_thread *main_thread,
                                   struct thread_msg *msg) {

    int rtn;
    switch (msg->type) {
        case ADD_RUNTIME:
            CHECK_MSG_SIZE(msg, struct add_runtime_peer);
            struct add_runtime_msg *runtime_msg = msg->data;
            rtn = add_runtime_peer(runtime_msg);

            if (rtn < 0) {
                log_warn("Error adding runtime peer");
            }
            break;
        // TODO: case CONNECT_TO_RUNTIME
        case CREATE_THREAD:
            CHECK_MSG_SIZE(msg, struct create_thread_msg);
            worker_threads[n_worker_threads ] = create_worker_thread(create_msg,
                                                                     main_thread);
            if (worker_threads[n_worker_threads] == NULL) {
                log_error("Error creating thread");
            } else {
                n_worker_threads++;
            }
            break;
        // TODO: case DELETE_THREAD:
        case SEND_TO_RUNTIME:
            CHECK_MSG_SIZE(msg, struct send_to_runtime_msg);
            struct send_to_runtime_msg *forward_msg = msg->data;
            rtn = send_to_peer(forward_msg);

            if (rtn < 0) {
                log_warn("Error forwarding message to peer");
            }
            break;
        case MODIFY_ROUTE:
            CHECK_MSG_SIZE(msg, struct route_modification_msg);
            struct route_modification_msg *route_msg = msg->data;
            rtn = modify_route(route_msg);

            if (rtn < 0) {
                log_warn("Error modifying route");
            }
            break;
        case CREATE_MSU:
        case DELETE_MSU:
        case ADD_ROUTE:
        case DEL_ROUTE:
            log_error("Message (type: %d) meant for worker thread sent to main thread",
                       msg->type);
            break;
        default:
            log_error("Unknown message type %d delivered to main thread", msg->type);
            break;
    }
}

static int check_main_thread_queue(struct dedos_thread *main_thread) {

    struct thread_msg *msg =
            dequeue_thread_msg(main_thread);

    if (thread_msg == NULL) {
        return 0;
    }

    // Dequeue twice?
    int rtn = process_main_thread_msg(msg);
    free(thread_msg);
    if (rtn < 0) {
        log_error("Error processing thread msg");
    }
    return 0;
}

#define STAT_REPORTING_DURATION 1

int runtime_main_thread_loop(struct dedos_thread *self, struct dedos_thread *main_thread) {

    if (self != main_thread) {
        log_warn("Main thread loop entered with self != main_thread");
    }

    // TODO: Remove in favor of main thread as another worker
    if (init_main_thread(main_thread) != 0) {
        log_critical("Error initializing main thread");
        return -1;
    }

    //???: Request_init_config()?
    if (request_init_config()) {
        log_warn("Error requesting initial config");
    }

    struct timespec begin;
    clock_gettime(CLOCK_MONOTONIC, begin);

    struct timespec elapsed;
    while (1) {
        // Check for incoming data processing
        // TODO: Move check_runtime_sockets() to separate polling thread
        // so that we don't have to busy loop
        int rtn = check_runtime_epoll(false);
        if (rtn != 0) {
            log_info("Breaking from main runtime thread loop "
                     "due to epoll");
            break;
        }

        //TODO: Wait on thread queue semaphore
        // rtn = thread_wait(main_thread);

        int rtn = check_main_thread_queue(&main_thread->queue);
        if (rtn != 0) {
            log_info("Breaking from main runtime thread loop "
                     "due to thread queue");
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &elapsed);
        int time_spent = elapsed.tv_sec - begin.tv_sec;
        if (time_spent >= STAT_REPORTING_DURATION) {
            send_stats_to_controller();
            clock_gettime(CLOCK_MONOTONIC, &begin);
        }
    }
    destroy_runtime();
}
