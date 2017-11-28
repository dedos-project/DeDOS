/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <string.h>
#include <libgen.h>

#include "controller_stats.h"
#include "local_files.h"
#include "logging.h"
#include "controller_dfg.h"
#include "runtime_communication.h"
#include "cli.h"
#include "scheduling.h"
#include "dfg.h"
#include "dfg_reader.h"
#include "controller_mysql.h"

#define FILENAME_LEN 32

pthread_t cli_thread;

static void print_usage() {
    printf("Usage: global_controller -j /path/to/json [-o json_file] [ -p json_port ]\n");
}

int main(int argc, char *argv[]) {
    set_local_directory(dirname(argv[0]));

    int option = 0;
    char filename[FILENAME_LEN];
    memset(filename, '\0', FILENAME_LEN);
    char *output_filename = NULL;
    int output_port = -1;
    bool db = false;
    bool init_db = false;

    struct option longopts[] = {
        {"db", no_argument, 0, 0 },
        {"init-db", no_argument, 0, 0},
        {NULL, 0, 0, 0}
    };

    int opt_index;
    while ((option = getopt_long(argc, argv,"j:o:p", longopts, &opt_index)) != -1) {
        switch (option) {
            case 0:
                if (strcmp(longopts[opt_index].name, "db") == 0) {
                    db = true;
                } else if (strcmp(longopts[opt_index].name, "init-db") == 0) {
                    db = true;
                    init_db = true;
                } else {
                    print_usage();
                    exit(EXIT_FAILURE);
                }
                break;
            case 'j' : strcpy(filename, optarg);
                break;
            case 'o' : output_filename = optarg;
                break;
            case 'p' : output_port = atoi(optarg);
                break;
            case 'd' : init_db = true;
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

    int rtn;

    rtn = init_dfg_lock();
    if (rtn < 0) {
        log_error("Error initializing DFG lock");
        return -1;
    }

    rtn = init_controller_dfg(filename);
    if (rtn < 0) {
        log_error("Error loading dfg!");
        return -1;
    }

    if (db) {
        rtn = db_init(init_db);
        if (rtn < 0) {
            log_error("Error setting up mysql DB!");
            return -1;
        }
    }

    rtn = init_statistics();

    if (rtn < 0) {
        log_error("Error initializing statistics");
        return -1;
    }
    int port = local_listen_port();

    //const char *policy = "greedy";
    //init_scheduler(policy);

    start_cli_thread(&cli_thread);
    runtime_communication_loop(port, output_filename, output_port);
    //start_communication(tcp_control_listen_port, output_filename);

    pthread_join(cli_thread, NULL);


    return 0;
}
