/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania

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
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <strings.h>

#include "controller_stats.h"
#include "scheduling.h"
#include "logging.h"
#include "communication.h"
#include "cli.h"
#include "dedos_msu_msg_type.h"
#include "runtime_messages.h"
#include "controller_dfg.h"
#include "runtime_communication.h"
#include "dfg.h"
#include "api.h"

#define NEXT_MSU_LOCAL 1
#define NEXT_MSU_REMOTE 2

#define BLD "\e[1m"
#define UL "\e[4m"
#define NOSTYLE "\e[0m"

#define HELP_PREAMBLE \
    "\nList of available commands : \n" \
    "\n" \
    "\t******* Note: " UL "ARGUMENTS" NOSTYLE " fields are required, excluding " \
        UL "[BRACKETED]" NOSTYLE "  ****\n" \
    "\n"


struct cmd_action {
    char cmd[32];
    int (*action)(char*);
    char args[32][16];
    char help[256];
};

static void parse_cmd_action(char *cmd);

static int parse_show_runtimes(char *args) {
    struct dedos_dfg *dfg = get_dfg();
    for (int i=0; i<dfg->n_runtimes; i++) {
        int fd = runtime_fd(dfg->runtimes[i]->id);
        printf("Runtime id: %d -- ", dfg->runtimes[i]->id);
        if (fd <= 0) {
            printf("NOT CONNECTED\n");
        } else {
            printf("fd: %d\n", fd);
        }
    }
    return 0;
}

#define NEXT_ARG(arg, args) \
    if ( ( arg = strtok(args, " \r\n") ) == NULL){ \
        log_error("Missing required argument"); \
        return -1; \
    }

/**
 * //FIXME: broken behavior since dfg_vertex members are not pointers anymore
 * Get a list of all MSU not possessing a "scheduling" object, and ask for an allocation plan
 * @param none
 * @return none
 */
static int parse_allocate() {
    // Pick only msus where no scheduling item is initialized
    //struct dfg_config *dfg;
    //dfg = get_dfg();

    struct to_schedule *ts = NULL;
    ts = malloc(sizeof(struct to_schedule));
    if (ts == NULL) {
        debug("Could not allocate memory for to_schedule");
        return -1;
    }

    int to_allocate_msus[MAX_MSU];
    int num_to_alloc = 0;

    //int n;
    /*
    for (n = 0; n < dfg->vertex_cnt; ++n) {
        if (dfg->vertices[n]->scheduling == NULL) {
            to_allocate_msus[n] = dfg->vertices[n]->msu_id;
            num_to_alloc++;
        }
    }
    */

    ts->msu_ids = to_allocate_msus;
    ts->num_msu = num_to_alloc;

    //TODO: allocate(ts);
    return 0;
}

/**
 * Display controller's time series for a given msu
 * @param *args: string received from the CLI
 * @return none
 */
static int parse_show_stats(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int msu_id = atoi(arg);

    struct dfg_msu *msu = get_dfg_msu(msu_id);
    if (msu ==  NULL) {
        printf("MSU id %d is not registered with the controller\n", msu_id);
        return -1;
    }

    show_stats(msu);
    return 0;
}

static int parse_show_msus(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int runtime_id = atoi(arg);
    struct dfg_runtime *rt = get_dfg_runtime(runtime_id);

    if (rt == NULL) {
        printf("Could not retrieve runtime %d from DFG", runtime_id);
        return -1;
    }

    //TODO: int rtn = send_report_msus_msg(rt);

    struct dedos_dfg *dfg = get_dfg();
    for (int i=0; i<dfg->n_msus; i++) {
        struct dfg_msu *msu = dfg->msus[i];
        if (msu->scheduling.runtime == rt) {
            printf("ID: %d\t  Thread: %d \t Type: %s (%d)\n",
                    msu->id, msu->scheduling.thread->id, msu->type->name, msu->type->id);
        }
    }


    int rtn = 0;

    if (rtn < 0) {
        printf("Could not send report MSUs message to runtime %d", runtime_id);
        return -1;
    }

    return 0;
}

static int parse_add_msu(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int runtime_id = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_type = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    char *msu_mode;
    NEXT_ARG(msu_mode, NULL);

    int thread_id;
    NEXT_ARG(arg, NULL);
    thread_id = atoi(arg);

    char *vertex_type = strtok(NULL, "\r\n|");
    if (vertex_type == NULL) {
        vertex_type = "";
    }

    char *init_data;
    char data[MAX_INIT_DATA_LEN];
    bzero(data, MAX_INIT_DATA_LEN);

    init_data = strtok(NULL, "\r\n");
    if (init_data != NULL) {
        debug("init data: %s, len: %zu", init_data, strlen(init_data));
        memcpy(data, init_data, strlen(init_data));
    }

    int ret = add_msu(msu_id, msu_type, data, msu_mode, vertex_type, thread_id, runtime_id);
    if (ret == -1) {
        log_error("Could not trigger new MSU creation");
    }

    return ret;
}

static int parse_del_msu(char *args) {
    char *arg;
    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    int rtn = remove_msu(msu_id);

    if ( rtn < 0 ){
        log_error("Could not deletion of MSU %d", msu_id);
    }
    return rtn;
}

static int parse_add_route(char *args) {
    char *arg;
    NEXT_ARG(arg, NULL);
    int route_id = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    int rtn = add_route_to_msu(route_id, msu_id);
    if ( rtn < 0 ) {
        log_error("Could not add route %d to msu %d", route_id, msu_id);
    }
    return rtn;
}

static int parse_del_route(char *args) {
    char *arg;
    NEXT_ARG(arg, NULL);
    int route_id = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    // TODO: int rtn = del_route_from_msu(msu_id, route_id);
    int rtn = 0;
    if ( rtn < 0 ) {
        log_error("Could not delete route %d from msu %d", route_id, msu_id);
    }
    return rtn;
}

static int parse_add_endpoint(char *args) {
    char *arg;
    NEXT_ARG(arg, NULL);
    int route_id = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    NEXT_ARG(arg, NULL);
    uint32_t key = (uint32_t)atoi(arg);

    int rtn = add_endpoint(msu_id, route_id, key);
    if (rtn < 0) {
        log_error("Could not add endpoint %d to route %d", msu_id, route_id);
    }
    return rtn;
}

static int parse_del_endpoint(char *args) {
    char *arg;
    NEXT_ARG(arg, NULL);
    int route_id = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    int rtn = del_endpoint(msu_id, route_id);
    if (rtn < 0) {
        log_error("Could not delete endpoint %d from route %d", msu_id, route_id);
    }
    return rtn;
}

static int parse_mod_endpoint(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int route_id = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    NEXT_ARG(arg, NULL);
    uint32_t key = (uint32_t)atoi(arg);

    int rtn = mod_endpoint(msu_id, route_id, key);
    if (rtn < 0) {
        log_error("Could not modify range for endpoiont %d on route %d to %u",
                msu_id, route_id, key);
    }
    return rtn;
}


static int parse_create_thread(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int runtime_id = atoi(arg);

    NEXT_ARG(arg, NULL);
    int thread_id = atoi(arg);

    char *mode;
    NEXT_ARG(mode, NULL);

    int rtn = create_worker_thread(thread_id, runtime_id, mode);

    if ( rtn < 0 ) {
        log_error("Could not create new worker thread on runtime %d", runtime_id);
    }

    return rtn;
}

static int parse_load_cfg(char *args) {
    char *filename;
    NEXT_ARG(filename, args);

    FILE *f = fopen(filename, "r");

    if ( f == NULL ) {
        log_error("Could not open cfg file %s", filename);
        return -1;
    }

    char *line = NULL;
    size_t len = 0;
    int read;
    while ( ( read = getline(&line, &len, f) ) != -1 ) {
        parse_cmd_action(line);
        sleep(1);
    }

    fclose(f);
    free(line);

    return 0;
}

static int parse_clone_msu(char *args) {
    char *msu_c;
    NEXT_ARG(msu_c, args);

    int id = atoi(msu_c);

    struct dfg_msu *to_clone = get_dfg_msu(id);
    if (to_clone == NULL) {
        log_error("MSU %d does not exist", id);
        return -1;
    }
    struct dfg_msu *cloned = clone_msu(id);
    if (cloned == NULL) {
        log_error("Could not clone msu %d", id);
        return -1;
    }
    return 0;
}

static int parse_show_routes(char *args) {
    char *msu_c;
    NEXT_ARG(msu_c, args);

    int id = atoi(msu_c);

    struct dfg_msu *msu = get_dfg_msu(id);

    if (msu == NULL) {
        log_error("Msu %d does not exist", id);
        return -1;
    }
    for (int i=0; i<msu->scheduling.n_routes; i++) {
        struct dfg_route *route = msu->scheduling.routes[i];
        printf("Route: %d\tType: %d\n", route->id, route->msu_type->id);
    }
    return 0;
}

static int parse_show_route(char *args) {
    char *route_id_c;
    NEXT_ARG(route_id_c, args);

    int id = atoi(route_id_c);

    struct dfg_route *route = get_dfg_route(id);
    if (route == NULL) {
        log_error("Route %d does not exist", id);
        return -1;
    }

    for (int i=0; i<route->n_endpoints; i++) {
        struct dfg_route_endpoint *ep = route->endpoints[i];
        printf("Destination: %d\tKey: %d\n", ep->msu->id, (int)ep->key);
    }
    return 0;
}


static int parse_help(char *cmd);

struct cmd_action cmd_actions[] = {
    {"show runtimes", parse_show_runtimes, {},
        "List connected runtimes and socket num"},

    {"show msus", parse_show_msus, {"RUNTIME-ID"},
        "Get MSUs running on the runtime" },

    {"show stats", parse_show_stats, {"MSU-ID"},
        "display stored time serie for a given msu"},

    {"show routes", parse_show_routes, {"MSU-ID"},
        "display routes attached to a given msu"},

    {"show route", parse_show_route, {"MSU-ID"},
        "display endpoints of a given route"},

    {"clone msu", parse_clone_msu, {"MSU-ID"},
        "clones and schedules the clone of an MSU"},

    {"add msu", parse_add_msu,
        {"RUNTIME-ID", "MSU_TYPE", "MSU-ID", "MSU_MODE", "THREAD-ID", "[VERTEX_TYPE]",
            "[ | INIT_DATA]"},
        "(MSU_MODE: blocking/non-blocking; VERTEX_TYPE: entry/exit/entry,exit)"},

    {"del msu", parse_del_msu, {"MSU-ID"}, "Delete MSU"},

    {"add route", parse_add_route,
        {"ROUTE-ID", "MSU-ID"},
        "Adds an outgoing route to an MSU"},

    {"del route", parse_del_route,
        {"ROUTE-ID", "MSU-ID"},
        "Deletes an outgoing route from an MSU"},

    {"add endpoint", parse_add_endpoint,
        {"ROUTE-ID", "MSU-ID", "KEY"},
        "Adds an MSU as an endpoint to a given route"},

    {"del endpoint", parse_del_endpoint,
        {"ROUTE-ID", "MSU-ID"},
        "Deletes an MSU as an endpoint from a given route"},

    {"mod endpoint", parse_mod_endpoint,
        {"ROUTE-ID", "MSU-ID", "KEY"},
        "Modifies the key range associated with an MSU endpoint on the given route"},

    {"add thread", parse_create_thread,
        {"RUNTIME-ID", "THREAD-ID", "MODE"},
        "(MODE=pinned/unpinned) Creates a worker thread on an unused core"},

    {"*allocate", parse_allocate, {},
        "gather all msu not possessing a 'scheduling' object, and compute a placement"},

    {"loadcfg", parse_load_cfg, {"FILENAME"},
        "load a suite of commands from a file"},

    {"help", parse_help, {}, "display available commands"},

    {"quit", NULL, {}, ""},

    {"\0", NULL, {}, "\0"}
};

//TODO: need to check whether an msu or a route are already present in the CFG before proceeding to action
static void parse_cmd_action(char *cmd) {
    size_t ln = strlen(cmd) - 1;
    if (*cmd && cmd[ln] == '\n') {
        cmd[ln] = '\0';
    }

    char cmd_cpy[256];
    strcpy(cmd_cpy, cmd);

    int rtn = -1;
    for ( int i = 0; cmd_actions[i].cmd[0] != '\0'; i++ ){
        if ( strncasecmp(cmd, cmd_actions[i].cmd, strlen(cmd_actions[i].cmd)) == 0 ) {
            if ( cmd_actions[i].action ) {
                rtn = cmd_actions[i].action(cmd + strlen(cmd_actions[i].cmd) + 1 );
            } else {
                rtn = 1;
            }
            break;
        }
    }
    if (rtn < 0){
        log_error("Error parsing command: %s", cmd_cpy);
    }

    return;
}


static int parse_help(char *args) {
    printf(HELP_PREAMBLE);
    for (int i=0; cmd_actions[i].cmd[0] != '\0'; i++){
        printf("\t" BLD "%s" NOSTYLE, cmd_actions[i].cmd);
        for (int j=0; cmd_actions[i].args[j][0]!='\0'; j++) {
            printf(" " UL "%s" NOSTYLE, cmd_actions[i].args[j]);
        }
        printf("\n\t\t%s\n", cmd_actions[i].help);
    }
    return 0;
}

static void* cli_loop() {
    int next = 0;
    char *line = NULL;
    size_t size;

    parse_help(NULL);

    do {
        printf("> Enter command: ");
        next = getline(&line, &size, stdin);
        if (next != -1) {
            if (strcmp(line, "quit\n") == 0) {
                //TODO CLEAN EXIT
                exit(0);
            }
            parse_cmd_action(line);
        }
    } while (next > 0);

    return NULL;
}

int start_cli_thread(pthread_t *cli_thread) {
    int err;
    err = pthread_create(cli_thread, NULL, cli_loop, NULL);
    if (err != 0) {
        debug("ERROR: can't create thread :[%s]", strerror(err));
    } else {
        debug("CLI Thread created successfully");
    }
    return err;
}
