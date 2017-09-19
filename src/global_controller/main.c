#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <string.h>
#include "logging.h"
#include "controller_dfg.h"
#include "runtime_communication.h"
#include "cli.h"
#include "scheduling.h"
#include "dfg.h"
#include "dfg_reader.h"

#define FILENAME_LEN 32

pthread_t cli_thread;

static void print_usage() {
    printf("Usage: global_controller -j /path/to/json [-o json_output]\n");
}

int main(int argc, char *argv[]) {
    int option = 0;
    char filename[FILENAME_LEN];
    memset(filename, '\0', FILENAME_LEN);
    char *output_filename = NULL;

    while ((option = getopt(argc, argv,"j:o:")) != -1) {
        switch (option) {
            //strncpy results in a segfault here.
            //e.g strncpy(filename, optarg, FILENAME_LEN);
            case 'j' : strcpy(filename, optarg);
                break;
            case 'o' : output_filename = optarg;
                break;
            default:
                print_usage();
                exit(EXIT_FAILURE);
        }
    }
    if (strlen(filename) == 0) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    int rtn = init_controller_dfg(filename);
    if (rtn < 0){
        log_error("Error loading dfg!");
        return -1;
    }

    int port = local_listen_port();

    //const char *policy = "greedy";
    //init_scheduler(policy);

    start_cli_thread(&cli_thread);
    runtime_communication_loop(port);
    // TODO: Output dfg to file
    (void)output_filename;
    //start_communication(tcp_control_listen_port, output_filename);

    pthread_join(cli_thread, NULL);


    return 0;
}
