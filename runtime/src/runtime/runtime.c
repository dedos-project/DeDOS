
#ifndef LOG_RUNTIME_INIT
#define LOG_RUNTIME_INIT 0
#endif

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


void runtime_main_thread_loop() {
    struct dedos_thread main_thread;
    if (init_main_thread(&main_thread) != 0) {
        log_critical("Error initializing main thread! Exiting!");
        return -1;
    }

    if (init_msu_types() != 0) {
        log_warn("Error initializing MSU types");
    }

    //???: Request_init_config()? 
    if (request_init_config()) {
        log_warn("Error requesting initial config");
    }

    struct timespec begin;
    get_elapsed_time(&begin);

    struct timespec elapsed;
    while (1) {
        // Check for incoming data processing
        int rtn = check_comm_sockets();
        if (rtn != 0) {
            log_info("Breaking from main runtime thread loop");
            break;
        }

        rtn = check_runtime_
