#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include "msu_ids.h"
#include "runtime_communication.h"
#include "logging.h"
#define SOCAT "/usr/bin/socat"

char *socat_cmd[] = {SOCAT, "stdio", "/tmp/haproxy.socket", NULL};

#define SOCAT_INPUT "set weight https/s%d %d\r\n"

#define WRITE_END 1
#define READ_END 0

int reweight_haproxy(int server, int weight){

    int fd[2];
    int rtn = pipe(fd);
    if (rtn < 0) {
        log_error("Error creating pipe");
        return -1;
    }

    pid_t pid = fork();

    if (pid == 0) {
        dup2(fd[READ_END], STDIN_FILENO);
        close(fd[WRITE_END]);
        execv(SOCAT, socat_cmd);
        close(fd[READ_END]);
        log_error("Failed to execute '%s'\n", SOCAT);
        exit(1);
    } else {
       int status;
       FILE *f = fdopen(fd[WRITE_END], "w");
       if (f == NULL) {
           log_error("Error opening FILE to pipe");
       }
       int rtn = fprintf(f, SOCAT_INPUT, server, weight);
       if (rtn == -1) {
           perror("fprintf");
       }
       fclose(f);
       close(fd[READ_END]);
       log_info("written");
       waitpid(pid, &status, 0);
    }
    return 0;
}

void set_haproxy_weights(int rt_id, int offset) {
    int n_reads[MAX_RUNTIMES+1];
    for (int i=0; i< MAX_RUNTIMES + 1; i++) {
        if (runtime_fd(i) > 0) {
            if (i == rt_id) {
                n_reads[i] = -offset;
            } else {
                n_reads[i] = 0;
            }
        } else {
            n_reads[i] = -1;
        }
    }
    struct dfg_msu_type *type = get_dfg_msu_type(WEBSERVER_READ_MSU_TYPE_ID);
    if (type == NULL) {
        log_error("Error getting read type");
        return;
    }
    for (int i=0; i<type->n_instances; i++) {
        int rt_id = type->instances[i]->scheduling.runtime->id;
        n_reads[rt_id]++;
    }

    for (int i=0; i <= MAX_RUNTIMES; i++) {
        if (n_reads[i] >= 0) {
            reweight_haproxy(i, n_reads[i]);
        }
    }
}


