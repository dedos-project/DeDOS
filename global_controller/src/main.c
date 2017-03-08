#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "logging.h"
#include "communication.h"
#include "cli_interface.h"

#include "dfg.h"

pthread_t cli_thread;

static void print_usage() {
    printf("Usage: global_controller -p tcp_control_listen_port\n");
}

int main(int argc, char *argv[]) {
    int tcp_control_listen_port;
    int option = 0;

    while ((option = getopt(argc, argv,"p:")) != -1) {
        switch (option) {
            case 'p' : tcp_control_listen_port = atoi(optarg);
                break;
            default:
                print_usage();
                exit(EXIT_FAILURE);
        }
    }
    if (tcp_control_listen_port < 1) {
        print_usage();
        exit(EXIT_FAILURE);
    }


    //char *policy = "greedy";
    //init_scheduler(policy);

    char *filename = "data/toload.json";
    do_dfg_config(filename);

    //pthread_t jl_thread;
    //start_json_listener_thread(&jl_thread);
    //test_serialization();
    

    start_cli_thread(&cli_thread);
    start_communication(tcp_control_listen_port);

    pthread_join(cli_thread, NULL);


    return 0;
}
