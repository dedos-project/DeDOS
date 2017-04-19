#pragma once
#include <time.h>
#include <pthread.h>

#define LOG_SELF_TIME 1
#define LOG_QUEUE_LEN 1
#define LOG_FULL_MSU_TIME  1
#define LOG_MSU_INTERNAL_TIME 1
#define LOG_THREAD_LOOP_TIME 1

enum stat_id{
    SELF_TIME = 0,
    QUEUE_LEN,
    MSU_FULL_TIME,
    MSU_INTERNAL_TIME,
    THREAD_LOOP_TIME,
    N_STAT_IDS
};

void flush_stat_to_log(enum stat_id stat_id, unsigned int item_id);

void flush_all_stats_to_log(int force);

void aggregate_end_time(enum stat_id stat_id, unsigned int item_id);

void aggregate_start_time(enum stat_id stat_id, unsigned int item_id);

void aggregate_stat(enum stat_id stat_id, unsigned int item_id, double stat, int relog);

void close_statlog();

void init_statlog(char *filename);

