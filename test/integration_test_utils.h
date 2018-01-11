#ifndef INTEGRATION_TEST_UTILS_H_
#define INTEGRATION_TEST_UTILS_H_

#define LOG_TEST
#define LOG_ALL 1

#include "assert.h"
#include "dedos_testing.h"
#include "runtime_dfg.h"
#include "msu_type.h"
#include "local_msu.h"
#include "msu_state.h"
#include "dfg_reader.h"
#include "communication.h"
#include "output_thread.h"
#include "socket_monitor.h"

#include <unistd.h>
#include <sys/wait.h>
#define TEST_RUNTIME_ID 1

#define MAX_N_RUNTIMES 16
static int rt_sockets[MAX_N_RUNTIMES];

struct mon_init {
    int local_port;
    struct sockaddr_in ctrl_addr;
};

static void* monitor_routine(void *mon_init_v) {
    struct mon_init *init = mon_init_v;
    log_info("Attempting to start socket monitor on port %d", init->local_port);
    run_socket_monitor(init->local_port, &init->ctrl_addr);
    free(init);
    pthread_exit(NULL);
    return NULL;
}

int start_up_runtime(struct dfg_runtime *rt, char *filename) {
    int rtn = init_runtime_dfg(filename, rt->id);
    if (rtn < 0) {
        log_error("Error initializing runtime %d", rt->id);
        return -1;
    }

    struct dedos_thread *output_thread = start_output_monitor_thread();
    if (output_thread == NULL) {
        log_critical("Error starting output thread on runtime %d", rt->id);
        return -1;
    }

    struct sockaddr_in ctrl_addr;
    controller_address(&ctrl_addr);

    pthread_t mon_thread;
    struct mon_init *mon_init = malloc(sizeof(*mon_init));
    mon_init->local_port = local_runtime_port();
    mon_init->ctrl_addr = ctrl_addr;
    log_info("Creating monitor thread for port %d", mon_init->local_port);
    pthread_create(&mon_thread, NULL, monitor_routine, mon_init);
    pthread_detach(mon_thread);
    return 0;
}

static void stop_runtime() {

    stop_output_monitor();
    stop_all_worker_threads();
    join_output_thread();
    free_runtime_dfg();
    // Parent waits on the execution of the test to exit
    log_info("Exiting runtime");
}


static int load_dfg_for_integration_test(char *filename) {

    log(LOG_TEST, "Loading DFG from %s", filename);
    struct dedos_dfg *dfg = parse_dfg_json_file(filename);

    if (dfg == NULL) {
        log_error("Error loading dfg %s", filename);
    }

    int ports[dfg->n_runtimes];
    for (int i = 0; i < dfg->n_runtimes; i++) {
        struct dfg_runtime *rt = dfg->runtimes[i];
        char ip[16];
        inet_ntop(AF_INET, &rt->ip, ip, sizeof(ip));
        if (strcmp(ip, "127.0.0.1") != 0 && strcmp(ip, "0.0.0.0") != 0) {
            log_error("All runtimes in test must have localhost IP");
            assert(false);
        }
        for (int j=0; j < i; j++) {
            if (ports[j] == rt->port) {
                log_error("All runtimes in test must have unique ports");
                assert(false);
            }
        }
        ports[i] = rt->port;
    }
    mark_point();

    int ctrl_socket = init_listening_socket(dfg->global_ctl_port);
    mark_point();

    for (int i = dfg->n_runtimes - 1 ; i > 0; i--) {
        // Spawn a child for the next runtime
        int pid = fork();
        if (pid < 0) {
            log_error("Error forking new process!");
            assert(false);
        }
        if (pid != 0) {
            close(ctrl_socket);
            int rtn = start_up_runtime(dfg->runtimes[i], filename);
            if (rtn == -1 ) {
                log_error("Error starting runtime %d", i);
                exit(-1);
            }
            int status;
            waitpid(pid, &status, 0);
            stop_runtime();
            exit(status);
        }
    }
    mark_point();
    int rtn = start_up_runtime(dfg->runtimes[0], filename);

    if (rtn == -1)  {
        log_error("Error starting runtime %d", 0);
        exit(-1);
    }
    mark_point();

    for (int i=0; i < dfg->n_runtimes; i++) {

        mark_point();
        int fd = accept(ctrl_socket, NULL, NULL);
        mark_point();

        if (fd < 0) {
            log_critical("Error accepting %dth runtime", i);
            exit(-1);
        }

        struct rt_controller_msg_hdr i_hdr;
        struct rt_controller_init_msg i_msg;
        mark_point();
        int rtn = read_payload(fd, sizeof(i_hdr), &i_hdr);
        if (i_hdr.type != RT_CTL_INIT) {
            log_error("Didn't get the right message type from runtime");
            exit(-1);
        }

        rtn = read_payload(fd, sizeof(i_msg), &i_msg);
        mark_point();

        if (rtn < 0) {
            log_error("Error reading payload %d", i);
            exit(-1);
        }

        for (int j=0; j < i; j++) {
            log_info("got connection from runtime %d at port %d", i_msg.runtime_id, i_msg.port);
            struct ctrl_add_runtime_msg msg = {
                .runtime_id = i_msg.runtime_id,
                .ip = i_msg.ip,
                .port = i_msg.port
            };

            struct ctrl_runtime_msg_hdr hdr = {
                .type = CTRL_CONNECT_TO_RUNTIME,
                .id = 0,
                .thread_id = 0,
                .payload_size = sizeof(msg)
            };

            send_to_endpoint(rt_sockets[j], &hdr, sizeof(hdr));
            send_to_endpoint(rt_sockets[j], &msg, sizeof(msg));
        }

        rt_sockets[i] = fd;
    }
    mark_point();
    free_dfg(dfg);
    return 0;
}

static void disconnect_from_runtimes() {
    for (int i=0; i < MAX_N_RUNTIMES; i++) {
        close(rt_sockets[i]);
    }
}

char DFG_JSON_FILE__[128];


int type_num_states(struct msu_type *type) {
    struct dfg_msu_type *dfg_type = get_dfg_msu_type(type->id);
    ck_assert_ptr_ne(dfg_type, NULL);
    int total_states = 0;
    for (int i=0; i < dfg_type->n_instances; i++) {
        int id = dfg_type->instances[i]->id;
        struct local_msu *msu = get_local_msu(id);
        total_states +=  msu_num_states(msu);
     }
    return total_states;
}


#define START_DEDOS_INTEGRATION_TEST(init) \
    START_DEDOS_TEST(init ##_integration_test_) { \
        mark_point(); \
        load_dfg_for_integration_test(DFG_JSON_FILE__); \
        usleep(10e3); \
        mark_point(); \
        init(); \
        usleep(10e3); \
        mark_point();

#define END_DEDOS_INTEGRATION_TEST() \
        disconnect_from_runtimes(); \
        stop_runtime(); \
    } END_DEDOS_TEST

#define DEDOS_START_INTEGRATION_TEST_LIST(json_filename) \
    DEDOS_START_TEST_LIST(json_filename); \
    set_local_directory("."); \
    init_statistics(); \
    DEDOS_ADD_TEST_RESOURCE(DFG_JSON_FILE__, json_filename); \


#define DEDOS_ADD_INTEGRATION_TEST_FN(fn) \
    DEDOS_ADD_TEST_FN(fn ##_integration_test_) \

#define DEDOS_END_INTEGRATION_TEST_LIST() \
    suite_add_tcase(s, tc); \
    SRunner *sr;\
    sr = srunner_create(s); \
    srunner_run_all(sr, CK_NORMAL); \
    int num_failed = srunner_ntests_failed(sr); \
    srunner_free(sr); \
    return (num_failed == 0) ? 0 : -1; \
    }

#endif
