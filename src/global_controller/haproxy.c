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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
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
        int dev_null = open("/dev/null", O_WRONLY);
        dup2(dev_null, STDOUT_FILENO);
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
       waitpid(pid, &status, 0);
    }
    return 0;
}

#define N_QLEN_SAMPLES 10

// TODO
//
/*
static int get_read_qlens(double *qlens, int n_qlens) {
    struct dfg_msu_type *type = get_dfg_msu_type(WEBSERVER_READ_MSU_TYPE_ID);
    if (type == NULL) {
        return 0;
    }
    for (int i=0; i < type->n_instances; i++) {
        struct dfg_msu *instance = type->instances[i];
        int rt_id = instance->scheduling.runtime->id;
        if (rt_id > n_qlens) {
            log_error("Cannot fit all runtimes in qlens");
            return -1;
        }
        struct timed_rrdb *stat = get_stat(MSU_QUEUE_LEN, instance->id);
        double mean = average_n(stat, N_QLEN_SAMPLES);
        if (qlens[rt_id] < 0) {
            qlens[rt_id] = 1;
        }
    }
}
*/
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


