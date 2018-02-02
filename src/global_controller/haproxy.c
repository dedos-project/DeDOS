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
#include "scheduling.h"

#include <math.h>
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
#include "controller_stats.h"


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
       close(fd[WRITE_END]);
    }
    log(LOG_HAPROXY, "Reweighted haproxy s%d -> %d", server, weight);
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

static int last_weights[MAX_RUNTIMES + 1] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static bool ignore_instances[MAX_MSU];

void set_haproxy_ignore_instance(int msu_id) {
    ignore_instances[msu_id] = true;
}

static double get_rt_limits_weight(int runtime_id) {
    double weight = 1;
    for (int i=0; i < N_REPORTED_STAT_TYPES; i++) {
        enum stat_id stat_id = reported_stat_types[i].id;
        double lim;
        int rtn = get_rt_stat_limit(runtime_id, stat_id, &lim);
        if (rtn != 0 || lim <= 0) {
            continue;
        }
        struct timed_rrdb *stat = get_runtime_stat(stat_id, runtime_id);
        if (stat == NULL || stat->used == 0) {
            continue;
        }
        double last_stat = stat->data[(stat->write_index + RRDB_ENTRIES - 1) % RRDB_ENTRIES];

        double this_weight = -.01 + 1.0 / ( 1 + exp(15 * ( (last_stat / lim) - .5)));
        weight *= this_weight;
    }
    log(LOG_LIMIT_WEIGHT, "Limit weight for runtime %d is %f", runtime_id, weight);
    if (weight < 0) {
        return 0;
    }
    return weight;
}

void set_haproxy_weights() {
    struct dfg_msu_type *type = get_dfg_msu_type(WEBSERVER_READ_MSU_TYPE_ID);
    if (type == NULL) {
        log_error("Error getting read type");
        return;
    }

    double q_lens[MAX_RUNTIMES + 1];
    for (int i=0; i < MAX_RUNTIMES + 1; i++) {
        q_lens[i] = 0;
    }
    int instances[MAX_RUNTIMES + 1];
    memset(instances, 0, sizeof(instances));
    for (int i=0; i < type->n_instances; i++) {
        struct dfg_msu *instance = type->instances[i];
        if (ignore_instances[instance->id]) {
            continue;
        }
        int rt_id = type->instances[i]->scheduling.runtime->id;
        instances[rt_id]++;
    }

    // Start at 1 to avoid dividing by 0 later
    double sum_qlen = 1;
    double max_qlen = 1;
    for (int i=0; i<type->n_instances; i++) {
        struct dfg_msu *instance = type->instances[i];
        if (ignore_instances[instance->id]) {
            continue;
        }
        double q_len = downstream_q_len(type->instances[i]);
        int rt_id = type->instances[i]->scheduling.runtime->id;
        q_lens[rt_id] += q_len / instances[rt_id];
        if (q_lens[rt_id] * 1.2 > max_qlen) {
            max_qlen = q_lens[rt_id] * 1.2;
        }
        if (q_lens[rt_id] == 0) {
            q_lens[rt_id] = 1;
        }
        sum_qlen += q_lens[rt_id];
    }




    double weights[MAX_RUNTIMES + 1];
    double max_weight = 1;
    for (int i=0; i < MAX_RUNTIMES + 1; i++) {
        if (instances[i]) {
            //weights[i] = (int)(255.0 * ((double)(max_qlen - q_lens[i])/(max_qlen)));
            //weights[i] = (int)(255.0 * ((double)(sum_qlen - q_lens[i])/(sum_qlen)));
            weights[i] = (double)((((sum_qlen))/q_lens[i])) * instances[i];
            weights[i] *= get_rt_limits_weight(i);
            if (weights[i] > max_weight) {
                max_weight = weights[i];
            }
        } else {
            weights[i] = 0;
        }
    }
    for (int i=0; i < MAX_RUNTIMES + 1; i++) {
        weights[i] = (255.0) * (weights[i] / max_weight);
        if (instances[i]) {
            log(LOG_HAPROXY, "WEIGHTS %d : %d", i, (int)weights[i]);
        }
    }

    for (int i=0; i <= MAX_RUNTIMES; i++) {
        if (last_weights[i] != (int)weights[i]) {
            reweight_haproxy(i, (int)weights[i]);
            last_weights[i] = (int)weights[i];
        }
    }

}


