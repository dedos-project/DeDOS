#ifndef INTEGRATION_TEST_UTILS_H_
#define INTEGRATION_TEST_UTILS_H_

#include "assert.h"
#include "dedos_testing.h"
#include "runtime_dfg.h"
#include "msu_type.h"
#include "local_msu.h"
#include "msu_state.h"
#include "dfg_reader.h"

#include <unistd.h>
#include <sys/wait.h>
#define TEST_RUNTIME_ID 1

static void load_dfg_for_integration_test(char *filename) {

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

    for (int i = dfg->n_runtimes - 1; i > 0; i--) {
        // Spawn a child for the next runtime
        int pid = fork();
        if (pid < 0) {
            log_error("Error forking new process!");
            assert(false);
        }
        if (pid != 0) {
            struct dfg_runtime *rt = dfg->runtimes[i];
            int rtn = init_runtime_dfg(filename, rt->id);
            if (rtn < 0) {
                log_error("Error initializing runtime %d", rt->id);
                assert(false);
            }
            // Parent waits on the execution of the test to exit
            waitpid(pid, NULL, 0);
            log_info("Exiting runtime %d", rt->id);
            exit(0);
        }
    }

    int rtn = init_runtime_dfg(filename, dfg->runtimes[0]->id);

    if (rtn < 0) {
        log_error("Error initializing runtime %d", dfg->runtimes[0]->id);
        assert(false);
    }
}

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

char DFG_JSON_FILE__[128];

#define START_DEDOS_INTEGRATION_TEST(init) \
    START_DEDOS_TEST(init ##_integration_test_) { \
        load_dfg_for_integration_test(DFG_JSON_FILE__); \
        usleep(100e3); \
        init(); \
        usleep(100e3);

#define END_DEDOS_INTEGRATION_TEST() \
    stop_all_worker_threads(); \
    } END_DEDOS_TEST

#define DEDOS_START_INTEGRATION_TEST_LIST(json_filename) \
    DEDOS_START_TEST_LIST(json_filename); \
    DEDOS_ADD_TEST_RESOURCE(DFG_JSON_FILE__, json_filename); \
    init_statistics(); \
    set_local_directory("."); \


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
