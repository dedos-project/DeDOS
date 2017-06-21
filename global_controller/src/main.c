#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <string.h>
#include "logging.h"
#include "communication.h"
#include "cli_interface.h"
#include "scheduling.h"
#include "dfg.h"
#include "dfg_reader.h"

#define FILENAME_LEN 32

pthread_t cli_thread;

static void print_usage() {
    printf("Usage: global_controller -p tcp_control_listen_port -j /path/to/json [-o json_output]\n");
}

int main(int argc, char *argv[]) {
    int tcp_control_listen_port = -1;
    int option = 0;
    char filename[FILENAME_LEN];
    memset(filename, '\0', FILENAME_LEN);
    char *output_filename = NULL;

    while ((option = getopt(argc, argv,"p:j:o:")) != -1) {
        switch (option) {
            case 'p' : tcp_control_listen_port = atoi(optarg);
                break;
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
    if (tcp_control_listen_port < 1 || strlen(filename) == 0) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    int rtn = load_dfg_from_file(filename);
    if (rtn < 0){
        log_error("Error loading dfg!");
        return -1;
    }

    //const char *policy = "greedy";
    //init_scheduler(policy);

    start_cli_thread(&cli_thread);
    start_communication(tcp_control_listen_port, output_filename);

    pthread_join(cli_thread, NULL);


    return 0;
}
