#ifndef STATS_IDS_H_
#define STATS_IDS_H_
#include <time.h>
#include <stdlib.h>

enum stat_id {
    MSU_QUEUE_LEN,
    MSU_ITEMS_PROCESSED,
    MSU_EXEC_TIME,
    MSU_IDLE_TIME,
    MSU_MEM_ALLOC,
    MSU_NUM_STATES,
    THREAD_CTX_SWITCHES,
    MSU_STAT1,
    MSU_STAT2,
    MSU_STAT3
};

#endif
