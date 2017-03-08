#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

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

static void send_route_update(char **input, int action){
    char *cmd = *input;
    int from_msu_id, to_msu_id, runtime_sock, from_msu_type, to_msu_type, to_msu_locality;
    char *ip_str;
    long total_msg_size = 0;
    int to_ip = 0;
    int ret;

    debug("DEBUG: Route update *input: %s", *input);
    debug("DEBUG: Route update cmd : %s", cmd);
    debug("DEBUG: Route update action: %u", action);

    runtime_sock = atoi(strtok(&cmd[9], " "));
    from_msu_id  = atoi(strtok(NULL, " "));
    from_msu_type = atoi(strtok(NULL, " "));
    to_msu_id = atoi(strtok(NULL, " "));
    to_msu_type = atoi(strtok(NULL, " "));
    to_msu_locality = atoi(strtok(NULL, " "));

    if (to_msu_locality == NEXT_MSU_REMOTE) {
        ip_str = strtok(NULL, "\r\n");
        debug("ip_str :%s", ip_str);
        string_to_ipv4(ip_str, &to_ip);
    }

    ret = update_route(action, runtime_sock, from_msu_id, from_msu_type,
                       to_msu_id, to_msu_type, to_msu_locality, to_ip);

    if (ret < 0 ) {
        debug("ERROR: %s", "Could not process update route request");
    }
}

//TODO: need to check whether an msu or a route are already present in the CFG before proceeding to action
static void parse_cmd_action(char *cmd)
{
    size_t ln = strlen(cmd) - 1;
    if (*cmd && cmd[ln] == '\n') {
        cmd[ln] = '\0';
    }
    debug("CMD: %s", cmd);
    char *buf;
    struct dedos_control_msg control_msg;

    if (!strcasecmp(cmd, "show runtimes")) {
        show_connected_peers();
    }
    else if (!strncasecmp(cmd, "loadcfg", 7)) {
        char *filename = strtok(&cmd[7], " ");
        char *line = NULL;
        size_t len = 0;
        FILE *f = fopen(filename, "r");
        int read;

        if (f == NULL) {
            debug("ERROR: could not open cfg file %s", filename);
        }

        while ((read = getline(&line, &len, f)) != -1) {
            parse_cmd_action(line);
            sleep(3);
        }

        fclose(f);
        free(line);
    }
    else if (!strncasecmp(cmd, "addmsu", 6)) {
        int runtime_sock, msu_type, msu_id;
        char *data;

        runtime_sock = atoi(strtok(&cmd[6], " "));

        struct dfg_vertex *new_msu = (struct dfg_vertex *) malloc(sizeof(struct dfg_vertex));
        if (new_msu == NULL) {
            debug("ERROR: %s", "could not allocate memory for new msu");
        }

        new_msu->msu_type = atoi(strtok(NULL, " "));
        new_msu->msu_id   = atoi(strtok(NULL, " "));

        data = strtok(NULL, "\r\n");
        //assume there is only the thread id after the mode
        memcpy(new_msu->msu_mode, data, strlen(data) - 2);

        new_msu->thread_id = atoi(data + strlen(new_msu->msu_mode) + 1);

        int ret;
        ret = add_msu(data, new_msu, runtime_sock);
        if (ret == -1) {
            debug("ERROR: %s", "could not trigger new msu creation");
        }

    }
    else if (!strncasecmp(cmd, "show msus", 9)) {

        int runtime_sock;
        control_msg.msg_type = REQUEST_MESSAGE;
        control_msg.msg_code = GET_MSU_LIST;
        control_msg.payload_len = 0;
        control_msg.header_len = sizeof(struct dedos_control_msg);

        runtime_sock = atoi(strtok(&cmd[9], " \r\n"));
        buf = malloc(sizeof(struct dedos_control_msg));
        memcpy(buf, &control_msg, sizeof(struct dedos_control_msg));

        send_to_runtime(runtime_sock, buf, sizeof(control_msg));

        free(buf);

    }
    else if (!strncasecmp(cmd, "delmsu", 6)) {
        // buf = (char*) malloc(sizeof(char) * ln);
        // strncpy(buf, cmd, ln);

        int runtime_sock;
        unsigned int msu_type;
        int id;
        char *data;
        int total_msg_size = 0;

        runtime_sock = atoi(strtok(&cmd[6], " "));
        msu_type = atoi(strtok(NULL, " "));
        id = atoi(strtok(NULL, " \r\n"));

        struct dedos_control_msg_manage_msu delete_msu_msg;
        delete_msu_msg.msu_id = id;
        delete_msu_msg.msu_type = msu_type;
        delete_msu_msg.init_data_size = 0;
        delete_msu_msg.init_data = NULL;

        debug("DEBUG: Sock %d\n", runtime_sock);
        debug("DEBUG: msu_type %d\n", delete_msu_msg.msu_type);
        debug("DEBUG: MSU id : %d\n", delete_msu_msg.msu_id);
        debug("DEBUG: init_data : %s\n", delete_msu_msg.init_data);

        control_msg.msg_type = ACTION_MESSAGE;
        control_msg.msg_code = DESTROY_MSU;
        control_msg.header_len = sizeof(struct dedos_control_msg); //might be redundant
        control_msg.payload_len = sizeof(struct dedos_control_msg_manage_msu) + delete_msu_msg.init_data_size;

        total_msg_size = sizeof(struct dedos_control_msg) + control_msg.payload_len;

        buf = (char*) malloc(total_msg_size);
        if (!buf) {
            debug("ERROR: Unable to allocate memory for sending control command. %s","");
            return;
        }
        memcpy(buf, &control_msg, sizeof(struct dedos_control_msg));
        memcpy(buf + sizeof(struct dedos_control_msg), &delete_msu_msg, sizeof(delete_msu_msg));

        //TODO Should also update it's actions in the dataflow graph
        send_to_runtime(runtime_sock, buf, total_msg_size);

        free(buf);

    }
    else if (!strncasecmp(cmd, "add route", 9)) {
        int action;
        action = MSU_ROUTE_ADD;
        send_route_update(&cmd, action);

    }
    else if (!strncasecmp(cmd, "del route", 9)) {
        int action;
        action = MSU_ROUTE_DEL;
        send_route_update(&cmd, action);

    }
    else if (!strncasecmp(cmd, "create_pinned_thread", 20)) {
        int runtime_sock, ret;
        runtime_sock = atoi(strtok(&cmd[20], "\r\n"));

        ret = create_worker_thread(runtime_sock);

        if (ret == -1) {
            debug("ERROR: %s %d", "could not create new worker thread on runtime", runtime_sock);
        }
    }

    return;
}

static void* cli_loop()
{
    int next = 0;
    char *line = NULL;
    size_t size;

    do {
        printf("\nList of available commands : \n"
                "\n"
                "\t*******NOTE: []are required fields else it will segfault*****\n"
                "\t*******NOTE: must create at least 1 pinned thread before creating MSUs*****\n"
                "\n"
                "\t1. show runtimes /* List connected runtimes and socket num */\n"
                "\t2. show msus [runtime_socket_num] /* Get MSUs running on the runtime */\n"
                "\t3. addmsu [runtime_socket_num] [msu_type] [msu_id_to_assign] [blocking/non-blocking] (thread num {1 to Current_pinned_threads} if non-blocking)\n"
                "\t4. delmsu [runtime_socket_num] [msu_type] [msu_id_to_delete]\n"
                "\t5. add route [which_on_runtime_socket_num] [on_this_msu_id] [this_msu_type] [to_msu_id] [to_msu_type] [1 for local, 2 for remote] (IP if remote)\n"
                "\t6. del route [which_on_runtime_socket_num] [on_this_msu_id] [this_msu_type] [to_msu_id] [to_msu_type] [1 for local, 2 for remote] (IP if remote)\n"
                "\t7. create_pinned_thread [runtime_socket_num] /* creates a pinned worker thread on a core not being used */\n"
                "\t8. loadcfg [filename] /* load a suite of commands from a file */\n"
                "\n"
        );

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

int start_cli_thread(pthread_t *cli_thread)
{
    int err;
    err = pthread_create(cli_thread, NULL, cli_loop, NULL);
    if (err != 0) {
        debug("ERROR: can't create thread :[%s]", strerror(err));
    } else {
        debug("INFO: %s", "CLI Thread created successfully");
    }
    return err;
}
