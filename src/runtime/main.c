/**
 * @file runtime/main.c
 * Main executable file to start the runtime
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>

#include "worker_thread.h"
#include "local_files.h"
#include "rt_stats.h"
#include "logging.h"
#include "runtime_dfg.h"
#include "controller_communication.h"
#include "main_thread.h"
#include "socket_monitor.h"

#define USAGE_ARGS \
    " -j dfg.json -i runtime_id [-l statlog]"

/**
 * Entry point to start the runtime
 */
int main(int argc, char **argv) {

    char *dfg_json = NULL;
    char *statlog = NULL;
    int runtime_id = -1;
    // TODO: tag_probability
    //char *tag_probability = NULL;

    struct option long_options[] = {
        {NULL, 0, 0, 0}
    };

    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "j:i:l:", long_options, &option_index);

        if (c == -1) {
            break;
        }
        switch (c) {
            case 'j':
                dfg_json = optarg;
                break;
            case 'i':
                runtime_id = atoi(optarg);
                break;
            case 'l':
                statlog = optarg;
                break;
            default:
                printf("Unknown option provided: %c. Exiting\n", c);
                exit(-1);
        }
    }

    if (dfg_json == NULL || runtime_id == -1) {
        printf("%s " USAGE_ARGS "\n", argv[0]);
        exit(-1);
    }
    set_local_directory(dirname(argv[0]));

    if (init_statistics() != 0) {
        log_warn("Error initializing runtime statistics");
    }

    if (init_runtime_dfg(dfg_json, runtime_id) != 0) {
        log_critical("Error initializing runtime %d  from json file %s. Exiting",
                  runtime_id, dfg_json);
        exit(-1);
    }

    struct sockaddr_in ctrl_addr;
    int rtn = controller_address(&ctrl_addr);
    if (rtn < 0) {
        log_critical("Could not get controller address from DFG");
        exit(-1);
    }

    // TODO: Initialize openssl in msu_type constructor for webserver
    struct dedos_thread *main_thread = start_main_thread();
    if (main_thread == NULL) {
        log_critical("Error starting main thread!");
        exit(-1);
    }

    // This will block until the socket monitor exits
    // TODO: Shut down main thread when socket monitor exits
    rtn = run_socket_monitor(local_runtime_port(), &ctrl_addr);
    if (rtn < 0) {
        log_critical("Error running socket monitor. Runtime will exit");
    }

    stop_all_worker_threads();
    finalize_statistics(statlog);
    log_info("Exiting runtime...");
    return 0;
}
