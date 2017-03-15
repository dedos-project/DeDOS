#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "logging.h"
#include "communication.h"
#include "cli_interface.h"
#include "scheduling.h"
#include "dfg.h"

#define FILENAME_LEN 32

pthread_t cli_thread;

static void print_usage() {
    printf("Usage: global_controller -p tcp_control_listen_port -j /path/to/json\n");
}

int main(int argc, char *argv[]) {
    int tcp_control_listen_port;
    int option = 0;
    char filename[FILENAME_LEN];
    memset(filename, '\0', FILENAME_LEN);

    while ((option = getopt(argc, argv,"p:j:")) != -1) {
        switch (option) {
            case 'p' : tcp_control_listen_port = atoi(optarg);
                break;
            //strncpy results in a segfault here.
            //e.g strncpy(filename, optarg, FILENAME_LEN);
            case 'j' : strcpy(filename, optarg);
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

    do_dfg_config(filename);

    start_cli_thread(&cli_thread);
    start_communication(tcp_control_listen_port);

    pthread_join(cli_thread, NULL);


    return 0;
}
