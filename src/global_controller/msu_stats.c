#include "msu_stats.h"
#include "timeseries.h"
#include "stats.h"
#include "logging.h"

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

static struct stat_type *get_stat_type(enum stat_id id) {
    for (int i=0; i < N_STAT_TYPES; i++) {
        if (stat_types[i].id == id) {
            return &stat_types[i];
        }
    }
    return NULL;
}

struct timed_rrdb *get_stat(enum stat_id id, unsigned int item_id) {
    if (!stats_initialized) {
        log_error("Stats not initialized");
        return NULL;
    }
    struct stat_type *type = get_stat_type(id);
    if (type == NULL) {
        log_error("Type %d not initialized", id);
        return NULL;
    }
    if (type->id_indices[item_id] == -1) {
        log_error("ID %u for types %d not initialized", item_id, id);
        return NULL;
    }
    return &type->items[type->id_indices[item_id]].stats;
}


int register_stat_item(unsigned int item_id) {
    if (!stats_initialized) {
        log_error("Stats not initialized");
        return -1;
    }
    log(LOG_STATS, "Allocating new msu stat item: %u", item_id);
    for (int i=0; i < N_STAT_TYPES; i++) {
        struct stat_type *type = &stat_types[i];
        if (type->id_indices[item_id] != -1) {
            log_warn("Item ID %u already assigned index %d",
                      item_id, type->id_indices[item_id]);
            return 1;
        }
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
        memset(&item->stats, 0, sizeof(item->stats));
    }
    return 0;
}

int init_statistics() {
    if (stats_initialized) {
        log_error("Statistics already initialized");
        return -1;
    }

    for (int i=0; i < N_STAT_TYPES; i++) {
        for (int j=0; j<MAX_STAT_ID; j++) {
            stat_types[i].id_indices[j] = -1;
        }
        stat_types[i].items = NULL;
    }
    stats_initialized = true;
    return 0;
}
