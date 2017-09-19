#ifndef MSU_STATS_H_
#define MSU_STATS_H_

#include "timeseries.h"
#include "stats.h"
#include "dfg.h"
#include "unused_def.h"

struct stat_item {
    unsigned int id;
    struct timed_rrdb stats;
};

#define MAX_STAT_ID 64

struct stat_type {
    enum stat_id id;
    char *name;
    int id_indices[MAX_STAT_ID];
    int num_items;
    struct stat_item *items;
};

static struct stat_type UNUSED stat_types[] = {
    {MSU_QUEUE_LEN, "QUEUE_LEN"},
    {MSU_ITEMS_PROCESSED, "ITEMS_PROCESSED"},
    {MSU_MEM_ALLOC, "MEMORY_ALLOCATED"}
};

#define N_STAT_TYPES sizeof(stat_types) / sizeof(*stat_types)

struct timed_rrdb *get_stat(enum stat_id id, unsigned int item_id);

int register_stat_item(unsigned int item_id);
int init_statistics();

void show_stats(struct dfg_msu *msu);

#endif
