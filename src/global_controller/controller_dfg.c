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
#include "dfg.h"
#include "logging.h"
#include "dfg_reader.h"
#include "controller_stats.h"
#include "haproxy.h"

#include <stdlib.h>

static struct dedos_dfg *DFG = NULL;

int init_controller_dfg(char *filename) {

    DFG = parse_dfg_json_file(filename);

    if (DFG == NULL) {
        log_error("Error initializing DFG");
        return -1;
    }
    set_dfg(DFG);

    set_haproxy_weights();

    return 0;
}

int local_listen_port() {
    if (DFG == NULL) {
        return -1;
    }
    return DFG->global_ctl_port;
}

static int max_msu_id = -1;

int generate_msu_id() {
    for (int i=0; i<DFG->n_msus; i++) {
        if (DFG->msus[i]->id > max_msu_id) {
            max_msu_id = DFG->msus[i]->id;
        }
    }
    max_msu_id++;
    return max_msu_id;
}

#define MAX_ROUTE_ID 9999

int generate_route_id() {
    for (int id = 0; id < MAX_ROUTE_ID; id++) {
        if (get_dfg_route(id) == NULL) {
            return id;
        }
    }
    return -1;
}

uint32_t generate_endpoint_key(struct dfg_route *route) {
    if (route->n_endpoints == 0) {
        return 1;
    }
    return route->endpoints[route->n_endpoints-1]->key + 1;
}

pthread_mutex_t dfg_mutex;

int init_dfg_lock() {
    return pthread_mutex_init(&dfg_mutex, NULL);
}

int lock_dfg() {
    int rtn = pthread_mutex_lock(&dfg_mutex);
    if (rtn < 0) {
        log_error("Error locking dfg");
        return -1;
    }
    return 0;
}

int unlock_dfg() {
    int rtn = pthread_mutex_unlock(&dfg_mutex);
    if (rtn < 0) {
        log_perror("Error unlocking dfg");
        return -1;
    }
    return 0;
}
struct dedos_dfg *get_dfg(void) {
    return DFG;
}
