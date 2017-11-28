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
#include "controller_stats.h"
#include "timeseries.h"
#include "stats.h"
#include "logging.h"
#include "controller_mysql.h"
#include "controller_dfg.h"
#include "stat_msg_handler.h"

#include <stdbool.h>

struct stat_type {
    enum stat_id id;
    char *name;
    int id_indices[MAX_STAT_ID];
    int num_items;
    struct stat_item *items;
};

static struct stat_type stat_types[] = {
    REPORTED_STAT_TYPES
};

#define N_STAT_TYPES sizeof(stat_types) / sizeof(*stat_types)
static bool stats_initialized = false;

#define CHECK_INIT \
    if (!stats_initialized) { \
        log_warn("Statistics not initialized"); \
        return -1; \
    }


static struct stat_type *get_stat_type(enum stat_id id) {
    for (int i=0; i < N_STAT_TYPES; i++) {
        if (stat_types[i].id == id) {
            return &stat_types[i];
        }
    }
    return NULL;
}

static struct timed_rrdb *get_stat(enum stat_id id, unsigned int item_id) {
    if (!stats_initialized) {
        log_error("Stats not initialized");
        return NULL;
    }
    if (item_id >= MAX_STAT_ID) {
        log_error("Item ID %u too high!", id);
        return NULL;
    }
    struct stat_type *type = get_stat_type(id);
    if (type == NULL) {
        log_error("Type %d not initialized", id);
        return NULL;
    }
    if (type->id_indices[item_id] == -1) {
        //log_error("ID %u for types %d not initialized", item_id, id);
        return NULL;
    }
    if (type->items == NULL) {
        return NULL;
    }
    return &type->items[type->id_indices[item_id]].stats;
}

struct timed_rrdb *get_thread_stat(enum stat_id id,
                                   unsigned int runtime_id, unsigned int thread_id) {
    if (!stats_initialized) {
        return NULL;
    }
    int item_id = runtime_id * MAX_THREADS + thread_id ;
    return get_stat(id, item_id);
}


struct timed_rrdb *get_msu_stat(enum stat_id id, unsigned int msu_id) {
    if (!stats_initialized) {
        return NULL;
    }
    return get_stat(id, msu_id);
}

static int unregister_stat(enum stat_id stat_id, unsigned int item_id) {
    if (!stats_initialized) {
        log_error("Stats not initialized");
        return -1;
    }
    struct stat_type *type = get_stat_type(stat_id);
    if (type->id_indices[item_id] == -1) {
        log_warn("Item ID %u not assigned", item_id);
        return -1;
    }
    type->id_indices[item_id] = -1;
    return 0;
}

int unregister_msu_stats(unsigned int msu_id) {
    CHECK_INIT;
    for (int i=0; i < N_REPORTED_MSU_STAT_TYPES; i++) {
        unregister_stat(reported_msu_stat_types[i].id, msu_id);
    }
    return 0;
}

int unregister_thread_stats(unsigned int thread_id, unsigned int runtime_id) {
    CHECK_INIT;
    for (int i=0; i < N_REPORTED_THREAD_STAT_TYPES; i++) {
        unregister_stat(reported_thread_stat_types[i].id, runtime_id * MAX_THREADS +  thread_id);
    }
    return 0;
}

int register_stat(enum stat_id stat_id, unsigned int item_id) {
    CHECK_INIT;
    if (item_id >= MAX_STAT_ID) {
        log_error("Item ID %u too high!", item_id);
        return -1;
    }
    if (!stats_initialized) {
        log_error("Stats not initialized");
        return -1;
    }
    struct stat_type *type = get_stat_type(stat_id);
    if (type->id_indices[item_id] != -1) {
        log_warn("Item ID %u already assigned index %d",
                  item_id, type->id_indices[item_id]);
        return 1;
    }
    log(LOG_STATS, "Allocating new msu stat item: %u", item_id);
    int index = type->num_items;
    type->id_indices[item_id] = index;
    type->num_items++;
    struct stat_item *new_item = realloc(type->items, sizeof(*type->items) * type->num_items);
    if (new_item != NULL) {
        type->items = new_item;
    } else {
        log_error("Error reallocating stat item");
        return -1;
    }

    struct stat_item *item = &type->items[index];
    item->id = item_id;
    for (int i=0; i < RRDB_ENTRIES; i++) {
        item->stats.time[i].tv_sec = -1;
    }
    memset(&item->stats, 0, sizeof(item->stats));

    return 0;
}

int register_msu_stats(unsigned int msu_id, int msu_type_id, int thread_id, int runtime_id) {
    CHECK_INIT;
    db_register_msu_stats(msu_id, msu_type_id, thread_id, runtime_id);
    for (int i=0; i < N_REPORTED_MSU_STAT_TYPES; i++) {
        if (register_stat(reported_msu_stat_types[i].id, msu_id)) {
            log_warn("Couldn't register msu %u", msu_id);
        }
    }
    return 0;
}

int register_thread_stats(unsigned int thread_id, unsigned int runtime_id) {
    CHECK_INIT;
    for (int i=0; i < N_REPORTED_THREAD_STAT_TYPES; i++) {
        if (register_stat(reported_thread_stat_types[i].id, runtime_id * MAX_THREADS + thread_id)) {
            log_warn("Couldn't register thread %u", thread_id);
        }
    }
    db_register_thread_stats(thread_id, runtime_id);
    return 0;
}

int init_statistics() {
    if (stats_initialized) {
        log_error("Statistics already initialized");
        return -1;
    }

    for (int i=0; i < N_REPORTED_STAT_TYPES; i++) {
        for (int j=0; j<MAX_STAT_ID; j++) {
            stat_types[i].id_indices[j] = -1;
        }
        stat_types[i].items = NULL;
    }
    stats_initialized = true;

    struct dedos_dfg *dfg = get_dfg();

    if (dfg == NULL) {
        log_error("DFG must be initialized before initializing statistics");
        return -1;
    }
    for (int i=0; i < dfg->n_runtimes; i++) {
        struct dfg_runtime *rt = dfg->runtimes[i];
        for (int j=0; j < rt->n_pinned_threads + rt->n_unpinned_threads; j++) {
            register_thread_stats(rt->threads[j]->id, rt->id);
        }
        // Also register output thread
        register_thread_stats(MAX_THREADS, rt->id);
    }
    for (int i=0; i < dfg->n_msus; i++) {
        register_msu_stats(dfg->msus[i]->id,
                           dfg->msus[i]->type->id,
                           dfg->msus[i]->scheduling.thread->id,
                           dfg->msus[i]->scheduling.runtime->id);
    }
    init_stats_msg_handler();
    return 0;
}


void show_stats(struct dfg_msu *msu){
    if (!stats_initialized) {
        return;
    }
    int stat_id = msu->id;
    for (int i=0; i < N_REPORTED_MSU_STAT_TYPES; i++) {
        struct timed_rrdb *ts = get_msu_stat(reported_msu_stat_types[i].id, stat_id);
        printf("******* Statistic: %s\n", reported_msu_stat_types[i].name);
        print_timeseries(ts);
    }
}
