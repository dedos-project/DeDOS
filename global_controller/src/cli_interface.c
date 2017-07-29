#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <strings.h>

#include "scheduling.h"
#include "logging.h"
#include "controller_tools.h"
#include "cli_interface.h"
#include "communication.h"
#include "dedos_msu_msg_type.h"
#include "control_protocol.h"
#include "dfg.h"
#include "api.h"

#define NEXT_MSU_LOCAL 1
#define NEXT_MSU_REMOTE 2

#define HELP_PREAMBLE \
    "\nList of available commands : \n" \
    "\n" \
    "\t*******NOTE: []are required fields else it will segfault*****\n" \
    "\t*******NOTE: must create at least 1 pinned thread before creating MSUs*****\n" \
    "\n"


struct cmd_action {
    char cmd[32];
    int (*action)(char*);
    char help[256];
};

static void parse_cmd_action(char *cmd);

static int parse_show_runtimes(char *args) {
    show_connected_peers();
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
static void parse_allocate() {
    // Pick only msus where no scheduling item is initialized
    struct dfg_config *dfg;
    dfg = get_dfg();

    struct to_schedule *ts = NULL;
    ts = malloc(sizeof(struct to_schedule));
    if (ts == NULL) {
        debug("Could not allocate memory for to_schedule");
        return;
    }

    int to_allocate_msus[MAX_MSU];
    int num_to_alloc = 0;

    int n;
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

    allocate(ts);
}

/**
 * Display controller's time series for a given msu
 * @param *args: string received from the CLI
 * @return none
 */
static void parse_show_stats(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int msu_id = atoi(arg);

    if (get_msu_from_id(msu_id) ==  NULL) {
        printf("MSU id %d is not registered with the controller\n", msu_id);
        return;
    }

    show_stats(msu_id);
}

static int parse_show_msus(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int runtime_sock = atoi(arg);


    struct dedos_control_msg control_msg = {
        .msg_type = REQUEST_MESSAGE,
        .msg_code = GET_MSU_LIST,
        .payload_len = 0,
        .header_len = sizeof(control_msg),
    };

    return send_control_msg(runtime_sock, &control_msg);

    return 0;
}

static int parse_add_msu(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int runtime_sock = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_type = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    char *msu_mode;
    NEXT_ARG(msu_mode, NULL);

    int thread_id = -1;
    if ( strcmp(msu_mode, "non_blocking") != 0 ){
        arg = strtok(NULL, " ");
        if ( arg == NULL)
            return -1;
        thread_id = atoi(arg);
    }

    char *init_data;
    char data[MAX_INIT_DATA_LEN];
    bzero(data, MAX_INIT_DATA_LEN);

    init_data = strtok(NULL, "\r\n");
    if (init_data != NULL) {
        debug("init data: %s, len: %zu", init_data, strlen(init_data));
        memcpy(data, init_data, strlen(init_data));
    }

    int ret = add_msu(data, msu_id, msu_type, msu_mode, thread_id, runtime_sock);
    if (ret == -1) {
        log_error("Could not trigger new MSU creation");
    }

    return ret;
}

static int parse_del_msu(char *args) {
    char *arg;

    NEXT_ARG(arg, args);
    int msu_type = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    int rtn = del_msu(msu_id, msu_type);
    if ( rtn < 0 ){
        log_error("Could not deletion of MSU %d", msu_id);
    }
    return rtn;
}

static int parse_add_route(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int runtime_sock = atoi(arg);

    NEXT_ARG(arg, NULL);
    int route_num = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    int rtn = add_route(msu_id, route_num, runtime_sock);
    if ( rtn < 0 ) {
        log_error("Could not add route %d to msu %d", route_num, msu_id);
    }
    return rtn;
}

static int parse_del_route(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int runtime_sock = atoi(arg);

    NEXT_ARG(arg, NULL);
    int route_num = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    int rtn = del_route(msu_id, route_num, runtime_sock);
    if ( rtn < 0 ) {
        log_error("Could not delete route %d from msu %d", route_num, msu_id);
    }
    return rtn;
}

static int parse_add_endpoint(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int runtime_sock = atoi(arg);

    NEXT_ARG(arg, NULL);
    int route_num = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    NEXT_ARG(arg, NULL);
    unsigned int range_end = (unsigned int)atoi(arg);

    int rtn = add_endpoint(msu_id, route_num, range_end, runtime_sock);
    if (rtn < 0) {
        log_error("Could not add endpoint %d to route %d", msu_id, route_num);
    }
    return rtn;
}

static int parse_del_endpoint(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int runtime_sock = atoi(arg);

    NEXT_ARG(arg, NULL);
    int route_num = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    int rtn = del_endpoint(msu_id, route_num, runtime_sock);
    if (rtn < 0) {
        log_error("Could not delete endpoint %d from route %d", msu_id, route_num);
    }
    return rtn;
}

static int parse_mod_endpoint(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int runtime_sock = atoi(arg);

    NEXT_ARG(arg, NULL);
    int route_num = atoi(arg);

    NEXT_ARG(arg, NULL);
    int msu_id = atoi(arg);

    NEXT_ARG(arg, NULL);
    unsigned int range_end = (unsigned int)atoi(arg);

    int rtn = mod_endpoint(msu_id, route_num, range_end, runtime_sock);
    if (rtn < 0) {
        log_error("Could not modify range for endpoiont %d on route %d to %l",
                msu_id, route_num, range_end);
    }
    return rtn;
}


static int parse_create_thread(char *args) {
    char *arg;
    NEXT_ARG(arg, args);
    int runtime_sock = atoi(arg);

    int rtn = create_worker_thread(runtime_sock);

    if ( rtn < 0 ) {
        log_error("Could not create new worker thread on runtime %d", runtime_sock);
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

static int parse_help(char *cmd);

struct cmd_action cmd_actions[] = {
    {"show runtimes", parse_show_runtimes,
        "/* List connected runtimes and socket num */"},

    {"show msus", parse_show_msus,
        "[runtime_socket_num] /* Get MSUs running on the runtime */" },

    {"addmsu", parse_add_msu,
        "[runtime_socket_num] [msu_type] [msu_id_to_assign] [blocking/non-blocking]"
        " (thread num {1 to Current_pinned_threads} if non-blocking)"},

    {"delmsu", parse_del_msu,
        "[msu_type] [msu_id_to_delete]"},

    {"add route", parse_add_route,
        "[runtime_socket_num] [route_num] [origin_msu]"
        " /* Adds an outgoing route to an MSU */"},

    {"del route", parse_del_route,
        "[runtime_socket_num] [route_num] [origin_msu]"
        " /* Deletes an outgoing route from an MSU */"},

    {"add endpoint", parse_add_endpoint,
        "[runtime_socket_num] [route_num] [destination_msu_id] [key_range_end]"
        " /* Adds an MSU as an endpoint to a given route */"},

    {"del endpoint", parse_del_endpoint,
        "[runtime_socket_num] [route_num] [destination_msu_id]"
        " /* Deletes an MSU as an endpoint from a given route */"},

    {"mod endpoint", parse_mod_endpoint,
        "[runtime_socket_num] [route_num] [destination_msu_id] [new_key_range_end]"
        " /* Modifies the key range associated with an MSU endpoint on the given route */"},

    {"create_pinned_thread", parse_create_thread,
        "[runtime_socket_num] /* Creates a pinned worker thread on an unused core */"},

    {"allocate", parse_allocate,
        "/* gather all msu not possessing a 'scheduling' object, and compute a placement */"},

    {"show stats", parse_show_stats,
        "[msu_id] /* display stored time serie for a given msu */"},

    {"loadcfg", parse_load_cfg,
        "[filename] /* load a suite of commands from a file */"},

    {"help", parse_help, "/* display available commands */"},

    {"quit", NULL, ""},

    {"\0", NULL, "\0"}
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
        printf("\t%d. %s %s\n", i, cmd_actions[i].cmd, cmd_actions[i].help);
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
