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

struct stat_item {
    unsigned int id;
    struct timed_rrdb min_stats;
    struct timed_rrdb max_stats;
};


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
        log_warn("ID %u for types %d not initialized", item_id, id);
        return NULL;
    }
    if (type->items == NULL) {
        return NULL;
    }
    return &type->items[type->id_indices[item_id]].min_stats;
}

static int runtime_item_id(int runtime_id) {
    return runtime_id;
}

struct timed_rrdb *get_runtime_stat(enum stat_id id,
                                    unsigned int runtime_id) {
    if (!stats_initialized) {
        return NULL;
    }
    return get_stat(id, runtime_item_id(runtime_id));
}

static int thread_item_id(int runtime_id, int thread_id) {
    return MAX_RUNTIMES + runtime_id * MAX_THREADS + thread_id;
}

struct timed_rrdb *get_thread_stat(enum stat_id id,
                                   unsigned int runtime_id, unsigned int thread_id) {
    if (!stats_initialized) {
        return NULL;
    }
    return get_stat(id, thread_item_id(runtime_id, thread_id));
}

static int msu_item_id(int msu_id) {
    return MAX_RUNTIMES + MAX_RUNTIMES * MAX_THREADS + msu_id;
}

struct timed_rrdb *get_msu_stat(enum stat_id id, unsigned int msu_id) {
    if (!stats_initialized) {
        return NULL;
    }
    return get_stat(id, msu_item_id(msu_id));
}

static int unregister_stat(enum stat_id stat_id, unsigned int item_id) {
    if (!stats_initialized) {
        log_error("Stats not initialized");
        return -1;
    }
    struct stat_type *type = get_stat_type(stat_id);
    if (type == NULL) {
        log_error("Cannot get stat type %d", stat_id);
        return -1;
    }
    if (type->id_indices[item_id] == -1) {
        log_warn("Item ID %u not assigned", item_id);
        return -1;
    }
    type->id_indices[item_id] = -1;
    return 0;
}

int unregister_msu_stats(unsigned int msu_id) {
    CHECK_INIT;
    for (int i=0; i < N_MSU_STAT_TYPES; i++) {
        unregister_stat(msu_stat_types[i].id, msu_item_id(msu_id));
    }
    return 0;
}

int unregister_thread_stats(unsigned int thread_id, unsigned int runtime_id) {
    CHECK_INIT;
    for (int i=0; i < N_THREAD_STAT_TYPES; i++) {
        unregister_stat(thread_stat_types[i].id, thread_item_id(runtime_id, thread_id));
    }
    return 0;
}

static int register_stat(enum stat_id stat_id, unsigned int item_id) {
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
    if (type == NULL) {
        log_error("Could not get stat type %d", stat_id);
        return -1;
    }
    if (type->id_indices[item_id] != -1) {
        log_warn("Item ID %u already assigned index %d",
                  item_id, type->id_indices[item_id]);
        return 1;
    }
    log(LOG_STAT_ALLOCATION, "Allocating new stat item: %d.%u", stat_id,item_id);
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
    memset(&item->min_stats, 0, sizeof(item->min_stats));
    memset(&item->max_stats, 0, sizeof(item->max_stats));

    return 0;
}

int register_runtime_stats(unsigned int runtime_id) {
    CHECK_INIT;
    // Database registration of stats performed on db_init 
    // b/c runtimes known in advance (for now)
    // db_register_rt_stats(runtime_id);
    for (int i=0; i < N_RUNTIME_STAT_TYPES; i++) {
        if (register_stat(runtime_stat_types[i].id, runtime_item_id(runtime_id))) {
            log_warn("Couldn't register runtime %u", runtime_id);
        }
    }
    return 0;
}

int register_msu_stats(unsigned int msu_id, int msu_type_id, int thread_id, int runtime_id) {
    CHECK_INIT;
    db_register_msu_stats(msu_id, msu_type_id, thread_id, runtime_id);
    for (int i=0; i < N_MSU_STAT_TYPES; i++) {
        if (register_stat(msu_stat_types[i].id, msu_item_id(msu_id))) {
            log_warn("Couldn't register msu %u", msu_id);
        }
    }
    return 0;
}

int register_thread_stats(unsigned int thread_id, unsigned int runtime_id) {
    CHECK_INIT;
    for (int i=0; i < N_THREAD_STAT_TYPES; i++) {
        if (register_stat(thread_stat_types[i].id, thread_item_id(runtime_id, thread_id))) {
            log_warn("Couldn't register thread %u", thread_id);
        } else {
            log(LOG_STAT_REGISTRATION,"Registered thread %u.%u (%d) ",
                runtime_id, thread_id, thread_item_id(runtime_id, thread_id));
        }
    }
    db_register_thread_stats(thread_id, runtime_id);
    return 0;
}

struct timespec start_time;

int init_statistics() {
    clock_gettime(CLOCK_REALTIME, &start_time);
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
        register_runtime_stats(rt->id);
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
    for (int i=0; i < N_MSU_STAT_TYPES; i++) {
        struct timed_rrdb *ts = get_msu_stat(msu_stat_types[i].id, stat_id);
        printf("******* Statistic %s:", msu_stat_types[i].label);;
        if (ts->used) {
            printf("\n");
            print_timeseries(ts, &start_time);
        } else {
            printf(" NO DATA\n");
        }
    }
}

int append_stat_sample(struct stat_sample *sample, int runtime_id) {
    int item_id;
    switch (sample->referent.type) {
        case THREAD_STAT:
            item_id = thread_item_id(sample->referent.id, runtime_id);
            break;
        case MSU_STAT:
            item_id = msu_item_id(sample->referent.id);
            break;
        case RT_STAT:
            item_id = runtime_item_id(sample->referent.id);
            break;
        default:
            log_error("Cannot locate referent type %d", sample->referent.type);
            return -1;
    }

    struct stat_type *type = get_stat_type(sample->stat_id);
    if (type == NULL) {
        log_error("Couldn't get stat type %d", sample->stat_id);
        return -1;
    }
    int idx;
    if ((idx = type->id_indices[item_id]) == -1) {
        log_warn("Item ID %u (referent %d) not assigned", item_id, sample->referent.id);
    }
    struct stat_item *item = &type->items[idx];

    struct timed_stat minstat = {sample->start, sample->bin_edges[0]};
    int rtn = append_to_timeseries(&minstat, 1, &item->min_stats);
    if (rtn < 0) {
        log_warn("Error appending to min timeseries");
    }
    struct timed_stat maxstat = {sample->start, sample->bin_edges[sample->n_bins]};
    rtn = append_to_timeseries(&maxstat, 1, &item->max_stats);
    if (rtn < 0) {
        log_warn("Error append to max timeseries");
    }
    return 0;
}


