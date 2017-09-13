/**
 * @file runtime/main.c
 * Main executable file to start the runtime
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdio.h>

#include "rt_stats.h"
#include "logging.h"
#include "runtime_dfg.h"
#include "controller_communication.h"
#include "main_thread.h"
#include "socket_monitor.h"

#define USE_OPENSSL

#define USAGE_ARGS \
    " -j dfg.json -i runtime_id [-z prof_tag_probability] [-l statlog]"

/**
 * Entry point to start the runtime
 */
int main(int argc, char **argv) {

#ifdef DATAPLANE_PROFILING
    log_warn("Data plane profiling enabled");
    init_data_plane_profiling();
#endif

    char *dfg_json = NULL;
    char *statlog = NULL;
    int runtime_id = -1;
    // TODO: tag_probability
    //char *tag_probability = NULL;

    struct option long_options[] = {
        {"prof-tag-probability", optional_argument, 0, 'z'},
        {NULL, 0, 0, 0}
    };

    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "j:i:z:", long_options, &option_index);

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
            case 'z':
                /*
                // TODO: Fix global dprof_tag_probability
                tag_probability = optarg;
                if(tag_probability != NULL){
                    sscanf(tag_probability, "%f", &dprof_tag_probability);
                }
                if(tag_probability == NULL || dprof_tag_probability < 0.0  ||
                        dprof_tag_probability > 1){
                    dprof_tag_probability = 1.0;
                }
                */
            default:
                printf("Unknown option provided: %c. Exiting\n", c);
                exit(-1);
        }
    }

    if (dfg_json == NULL || runtime_id == -1) {
        printf("%s " USAGE_ARGS "\n", argv[0]);
        exit(-1);
    }

    if (init_runtime_dfg(dfg_json, runtime_id) != 0) {
        log_critical("Error initializing runtime %d  from json file %s. Exiting",
                  runtime_id, dfg_json);
        exit(-1);
    }

    if (init_statistics() != 0) {
        log_warn("Error initializing runtime statistics");
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

    finalize_statistics(statlog);
    log_info("Exiting runtime...");
    return 0;
}
