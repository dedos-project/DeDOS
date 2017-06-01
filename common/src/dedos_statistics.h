#ifndef _DEDOS_STATISTICS_H_
#define _DEDOS_STATISTICS_H_
#include "logging.h"
#include "uthash.h"

//used for stat array
#define MAX_MSUS_PER_THREAD 10

struct msu_stats_data {
    int msu_id;
    unsigned int queue_item_processed;
    unsigned int memory_allocated;
    unsigned int data_queue_size;
};

//to keep direct pointer to entry in array where MSU stats are stored in the thread
struct index_tracker {
    int id; //msu_id key for hash table
    struct msu_stats_data *stats_data_ptr; //ptr in data array
    UT_hash_handle hh;
};

/* per work thread structure to collect statistics from all MSUs running
on the worker thread */
struct thread_msu_stats {
    unsigned int num_msus;
    int array_len;
    struct msu_stats_data *msu_stats_data;
    struct index_tracker *msu_stats_index_tracker;
    void *mutex;
};

int init_thread_stats(struct thread_msu_stats* thread_stats);
int init_aggregate_thread_stats(struct thread_msu_stats* thread_stats, unsigned int total_msus);
int update_aggregate_stats_size(struct thread_msu_stats* thread_stats, unsigned int total_worker_threads);
void destroy_thread_stats(struct thread_msu_stats* thread_stats);
void destroy_aggregate_thread_stats(struct thread_msu_stats* thread_stats);
int register_msu_with_thread_stats(struct thread_msu_stats* thread_stats, int msu_id);
int remove_msu_thread_stats(struct thread_msu_stats* thread_stats, int msu_id);
void print_thread_stats_entries(struct index_tracker *msu_stats_index_tracker);
struct index_tracker* find_index_tracking_entry(struct index_tracker *msu_stats_index_tracker, int msu_id);
void iterate_print_thread_stats_array(struct thread_msu_stats* thread_msu_stats);
int copy_stats_from_worker(unsigned int all_threads_index);
void print_aggregate_stats(void);

#endif
