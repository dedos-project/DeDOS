#include "stats.h"
#include <time.h>
#include <stdio.h>
#include <pthread.h>

#define MAX_STATS 2048
#define MAX_ITEM_ID 32
#define FLUSH_TIME_S 10

int log_mask[] = {
    LOG_SELF_TIME,
    LOG_QUEUE_LEN,
    LOG_FULL_MSU_TIME,
    LOG_MSU_INTERNAL_TIME,
    LOG_THREAD_LOOP_TIME
};


char *stat_format[] = {
    "%0.9f",
    "%02.0f",
    "%0.9f",
    "%0.9f",
    "%0.9f",
};

char *stat_name[] = {
    "_STAT_FLUSH_TIME",
    "MSU_QUEUE_LENGTH",
    "___MSU_FULL_TIME",
    "__MSU_INNER_TIME",
    "THREAD_LOOP_TIME"
};

struct item_stats
{
    time_t last_flush;
    unsigned int item_id;
    int n_stats;
    pthread_mutex_t mutex;
    struct timespec time[MAX_STATS];
    double stat[MAX_STATS];
};

struct dedos_stats{
    enum stat_id stat_id;
    struct item_stats item_stats[MAX_ITEM_ID];
};


struct dedos_stats saved_stats[N_STAT_IDS];

time_t start_time_s;
pthread_mutex_t log_mutex;
FILE *statlog;

void get_elapsed_time(struct timespec *t){
    clock_gettime(CLOCK_MONOTONIC, t);
    t->tv_sec -= start_time_s;
}

void flush_item_to_log(enum stat_id stat_id, unsigned int item_id){
    aggregate_start_time(SELF_TIME, stat_id);
    struct item_stats *item = &saved_stats[(int)stat_id].item_stats[item_id];
    int n_to_write[item->n_stats];
    char to_write[item->n_stats][128];
    pthread_mutex_lock(&item->mutex);
    for (int i=0; i<item->n_stats; i++){
        int written = sprintf(to_write[i], "%s:%02d:%05ld.%010ld:", 
                              stat_name[stat_id], 
                              item_id, 
                              item->time[i].tv_sec, item->time[i].tv_nsec);
        written += sprintf(to_write[i]+written, stat_format[(int)stat_id], item->stat[i]);
        n_to_write[i] = written;
    }
    struct timespec writetime;
    get_elapsed_time(&writetime);
    item->last_flush = writetime.tv_sec;

    item->time[0] = item->time[item->n_stats];
    int n_stats = item->n_stats;
    item->n_stats = 0;
    pthread_mutex_unlock(&item->mutex);

    pthread_mutex_lock(&log_mutex);
    for (int i=0; i<n_stats; i++){
        fwrite(to_write[i], sizeof(char), n_to_write[i], statlog);
        fwrite("\n", sizeof(char), 1, statlog);
    }
    pthread_mutex_unlock(&log_mutex);
    aggregate_end_time(SELF_TIME, stat_id);
}

void flush_all_stats_to_log(int force){
    aggregate_start_time(SELF_TIME, N_STAT_IDS);
    struct timespec curtime;
    get_elapsed_time(&curtime);
    int did_log = 0;
    for (int i=N_STAT_IDS-1; i>=0; i--){
        for (int j=0; j<MAX_ITEM_ID; j++){
            struct item_stats *item = &saved_stats[i].item_stats[j];
            pthread_mutex_lock(&item->mutex);
            int do_log = item->n_stats > 0 && 
                    (curtime.tv_sec - item->last_flush > FLUSH_TIME_S || force);
            pthread_mutex_unlock(&item->mutex);
            if (do_log){
                flush_item_to_log(i, j);
                did_log = 1;
            }
        }
    }
    if (did_log)
        aggregate_end_time(SELF_TIME, N_STAT_IDS);

}

void aggregate_end_time(enum stat_id stat_id, unsigned int item_id){
    if (log_mask[stat_id] == 0)
        return;
    struct timespec newtime;
    get_elapsed_time(&newtime);
    struct item_stats *item = &saved_stats[(int)stat_id].item_stats[item_id];
    pthread_mutex_lock(&item->mutex);
    time_t timediff_s = newtime.tv_sec - item->time[item->n_stats].tv_sec;
    long timediff_ns = newtime.tv_nsec - item->time[item->n_stats].tv_nsec;
    item->stat[item->n_stats] = (double)timediff_s + ((double)timediff_ns/(1000000000.0));
    item->n_stats++;
    int flush = item->n_stats == MAX_STATS;
    pthread_mutex_unlock(&item->mutex);
    if (flush){
        flush_item_to_log(stat_id, item_id);
    }
}

void aggregate_start_time(enum stat_id stat_id, unsigned int item_id){
    if (log_mask[stat_id] == 0)
        return;
    struct item_stats *item = &saved_stats[(int)stat_id].item_stats[item_id];
    pthread_mutex_lock(&item->mutex);
    get_elapsed_time(&item->time[item->n_stats]);
    pthread_mutex_unlock(&item->mutex);
}

void aggregate_stat(enum stat_id stat_id, unsigned int item_id, double stat, int relog){
    if (log_mask[stat_id] == 0)
        return;
    struct item_stats *item = &saved_stats[(int)stat_id].item_stats[item_id];
    pthread_mutex_lock(&item->mutex);
    if (item->stat[item->n_stats-1] != stat || relog){
        get_elapsed_time(&item->time[item->n_stats]);
        item->stat[item->n_stats] = stat;
        item->n_stats++;
    }
    int flush = item->n_stats == MAX_STATS;
    pthread_mutex_unlock(&item->mutex);
    if (flush){
        flush_item_to_log(stat_id, item_id);
    }
}

void init_statlog(char *filename){
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    start_time_s = start_time.tv_sec;
    for (int i=0; i<N_STAT_IDS; i++){
        for (int j=0; j<MAX_ITEM_ID; j++){
            pthread_mutex_init(&saved_stats[i].item_stats[j].mutex, NULL );
            saved_stats[i].item_stats[j].last_flush = 0;
            saved_stats[i].item_stats[j].n_stats = 0;
        }
    }
    statlog = fopen(filename, "w");
}

void close_statlog(){
    flush_all_stats_to_log(1);
    fclose(statlog);
}
