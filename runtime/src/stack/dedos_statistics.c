#include "runtime.h"
#include "dedos_statistics.h"

/*
 * Initialize worker thread statistic structure
 * @param thread_stats: pointer to freshly allocated memory for a thread_msu_stats struct
 * @returns 0 on success
 **/

int init_thread_stats(struct thread_msu_stats* thread_stats) {
    unsigned int i;
    thread_stats->msu_stats_index_tracker = NULL;
    thread_stats->num_msus = 0;
    thread_stats->array_len = MAX_MSUS_PER_THREAD;
    log_debug("thread_stats array len: %u", thread_stats->array_len);
    thread_stats->mutex = NULL;
    thread_stats->msu_stats_data = malloc(sizeof(struct msu_stats_data)* thread_stats->array_len);
    if (!thread_stats->msu_stats_data) {
        log_error("Failed to malloc thread_stats array%s","");
        return -1;
    }

    struct msu_stats_data* cur_stat = NULL;
    for (i = 0; i < thread_stats->array_len; i++) {
        //FIXME: preallocating the stat results in useless memory consumption
        cur_stat = &(thread_stats->msu_stats_data[i]);
        cur_stat->msu_id = -1;
        cur_stat->queue_item_processed[1] = 0;
        cur_stat->memory_allocated[1] = 0;
        cur_stat->data_queue_size[1] = 0;
    }

    return 0;
}

/*
 * Initialize main thread statistic structure
 * @param hread_stats: pointer to freshly allocated memory for a thread_msu_stats struct
 * @param total_msus: number of msu in the system
 * @return 0 on success
 **/

int init_aggregate_thread_stats(struct thread_msu_stats* thread_stats, unsigned int total_msus) {
    unsigned int i;
    thread_stats->msu_stats_index_tracker = NULL;
    thread_stats->num_msus = 0;
    thread_stats->array_len = total_msus;
    log_debug("thread_stats array len: %u", thread_stats->array_len);
    thread_stats->mutex = NULL;

    if (total_msus != 0) {
        thread_stats->msu_stats_data =
            malloc(sizeof(struct msu_stats_data) * thread_stats->array_len);
        if (!thread_stats->msu_stats_data) {
            log_error("Failed to malloc thread_stats array%s","");
            return -1;
        }

        struct msu_stats_data* cur_stat = NULL;
        for (i = 0; i < thread_stats->array_len; i++) {
            //FIXME: preallocating the stat results in useless memory consumption
            cur_stat = &(thread_stats->msu_stats_data[i]);
            cur_stat->msu_id = -1;
            cur_stat->queue_item_processed[1] = 0;
            cur_stat->memory_allocated[1] = 0;
            cur_stat->data_queue_size[1] = 0;
        }
    }

    return 0;
}

/*
 * Recreate an aggregate stats struct (typically called when a new worker thread is created)
 * @param thread_stats: the main thread's struct thread_msu_stats
 * @param total_worker_threads
 * @return 0 on success
 **/

int update_aggregate_stats_size(struct thread_msu_stats* thread_stats,
                                unsigned int total_worker_threads) {
    if (thread_stats->array_len != 0) {
        free(thread_stats->msu_stats_data);
    }

    int ret;
    ret = init_aggregate_thread_stats(thread_stats, MAX_MSUS_PER_THREAD * total_worker_threads);
    if (ret) {
        return -1;
    }

    log_debug("Updated aggregate stats size: new space for total msus : %u",
              MAX_MSUS_PER_THREAD * total_worker_threads);

    return 0;
}

void destroy_thread_stats(struct thread_msu_stats* thread_stats){
    // free(thread_stats->msu_stats_data);
    //TODO free msu_stats_index_tracker
    // free(thread_stats);
}

void destroy_aggregate_thread_stats(struct thread_msu_stats* thread_stats) {
    if (thread_stats->array_len > 0) {
        free(thread_stats->msu_stats_data);
    }
    free(thread_stats);
}

static int find_empty_slot(struct thread_msu_stats* thread_stats) {
    int i;
    log_debug("thread stats array_len: %u", thread_stats->array_len);

    for (i = 0; i < thread_stats->array_len; i++) {
        log_debug("Current entry: %d",thread_stats->msu_stats_data[i].msu_id);
        if (thread_stats->msu_stats_data[i].msu_id == -1) {
            log_debug("Found slot for new stat entry: %d",i);
            return i;
        }
    }

    if (i == thread_stats->array_len) {
        return -1;
    }

    return -1;
}

static struct index_tracker* add_index_tracking_entry(struct thread_msu_stats *thread_stats,
                                                      struct msu_stats_data *msu_stats_data_entry) {
    struct index_tracker *s, *test;
    struct index_tracker *tmp;

    log_debug("index tracking for id: %d", msu_stats_data_entry->msu_id);

    s = (struct index_tracker*)malloc(sizeof(struct index_tracker));
    if (!s) {
        log_error("Failed to malloc for index tracker struct %s","");
        return NULL;
    }

    s->id = msu_stats_data_entry->msu_id;
    s->stats_data_ptr = msu_stats_data_entry;

    HASH_ADD_INT(thread_stats->msu_stats_index_tracker, id, s );
    // thread_stats->msu_stats_index_tracker = tmp;
    log_debug("Added tracking entry for msu id: %d", s->id);
// #if DEBUG != 0
//     log_debug("Finding index_tracker_entry for msu: %d",s->id);
//     HASH_FIND_INT( msu_stats_index_tracker, &(s->id), test);
//     log_debug("found id: %d",test->id);
// #endif
    return thread_stats->msu_stats_index_tracker;
}

struct index_tracker* find_index_tracking_entry(struct index_tracker *msu_stats_index_tracker,
                                                int msu_id) {
    struct index_tracker *s;
    HASH_FIND_INT(msu_stats_index_tracker, &msu_id, s);

    return s;
}

static struct index_tracker* delete_index_tracking_entry(struct index_tracker *msu_stats_index_tracker,
                                                         int msu_id) {
    struct index_tracker *to_del;
    to_del = find_index_tracking_entry(msu_stats_index_tracker, msu_id);
    if (to_del == NULL) {
        log_warn("No stat tracking entry for msu id to delete: %d",msu_id);
        return msu_stats_index_tracker;
    }

    HASH_DEL(msu_stats_index_tracker, to_del);  /* user: pointer to deletee */
    free(to_del);

    return msu_stats_index_tracker;
}

void print_thread_stats_entries(struct index_tracker *msu_stats_index_tracker) {
    struct index_tracker *s;
    for (s=msu_stats_index_tracker; s != NULL; s=(struct index_tracker*)(s->hh.next)) {
        log_debug("msu id %d: stat_data->msu_id %d, queue_size: %u at time: %d",
                  s->id, s->stats_data_ptr->msu_id,
                  s->stats_data_ptr->data_queue_size[1],
                  s->stats_data_ptr->data_queue_size[0]);
    }
}

void iterate_print_thread_stats_array(struct thread_msu_stats* thread_msu_stats) {
    unsigned int i;
    for (i = 0; i < thread_msu_stats->array_len; i++) {
        if (thread_msu_stats->msu_stats_data[i].msu_id == -1) {
            continue;
        }

        log_stats("msu_id: %d, items_prcsd: %u, mem_usage: %u, q_size: %u",
                  thread_msu_stats->msu_stats_data[i].msu_id,
                  thread_msu_stats->msu_stats_data[i].queue_item_processed[1],
                  thread_msu_stats->msu_stats_data[i].memory_allocated[1],
                  thread_msu_stats->msu_stats_data[i].data_queue_size[1]);
    }
}

int register_msu_with_thread_stats(struct thread_msu_stats* thread_stats, int msu_id) {
    //find an empty slot in the array and init the stats
    int empty_index;
    int ret;
    struct index_tracker *tmp;

    mutex_lock(thread_stats->mutex);
    empty_index = find_empty_slot(thread_stats);
    if (empty_index == -1) {
        log_error("All stats slots are full num_msus: %d",thread_stats->num_msus);
        //TODO Resize when all slots are full
        return -1;
    }

    thread_stats->msu_stats_data[empty_index].msu_id = msu_id;
    thread_stats->num_msus++;

    thread_stats->msu_stats_data[empty_index].queue_item_processed[1] = 0;
    thread_stats->msu_stats_data[empty_index].memory_allocated[1] = 0;
    thread_stats->msu_stats_data[empty_index].data_queue_size[1] = 0;

    struct msu_stats_data* ptr = &(thread_stats->msu_stats_data[empty_index]);
    mutex_unlock(thread_stats->mutex);

    //create a hash table entry and point the ptr to the above location in the array
    tmp = add_index_tracking_entry(thread_stats, ptr);
    if (!tmp) {
        log_error("failed to add stat index tracker entry for msu id: %d",msu_id);
        return -1;
    }

    thread_stats->msu_stats_index_tracker = tmp;
    log_debug("Tracking stats for num msus: %u",thread_stats->num_msus);
    print_thread_stats_entries(thread_stats->msu_stats_index_tracker);

    return 0;
}

int remove_msu_thread_stats(struct thread_msu_stats* thread_stats, int msu_id) {
    //find ptr in array via index tracker
    struct index_tracker *index_tracker_entry;
    index_tracker_entry = find_index_tracking_entry(thread_stats->msu_stats_index_tracker, msu_id);
    if(!index_tracker_entry){
        log_error("Cannot find tracking entry for msu id: %d", msu_id);
        return -1;
    }

    struct msu_stats_data *stats_data_entry = index_tracker_entry->stats_data_ptr;
    if (msu_id != stats_data_entry->msu_id) {
        log_error("Mismatch in msu id in stats tracker and stats_data_ptr %s","");
        return -1;
    }

    mutex_lock(thread_stats->mutex);
    stats_data_entry->msu_id = -1;
    stats_data_entry->queue_item_processed[1] = 0;
    stats_data_entry->memory_allocated[1] = 0;
    stats_data_entry->data_queue_size[1] = 0;
    thread_stats->num_msus--;
    mutex_unlock(thread_stats->mutex);

    index_tracker_entry = delete_index_tracking_entry(thread_stats->msu_stats_index_tracker, msu_id);
    log_debug("Deleted stats tracking entry for msu id: %d", msu_id);
    if (thread_stats->num_msus == 0) {
        thread_stats->msu_stats_index_tracker = NULL;
    } else {
        thread_stats->msu_stats_index_tracker = index_tracker_entry;
    }
    print_thread_stats_entries(thread_stats->msu_stats_index_tracker);

    return 0;
}

int copy_stats_from_worker(unsigned int all_threads_index){

    //memcpy all stats from the worker to main threads stats struct at appropriate index
    unsigned int cpy_start_index = (all_threads_index - 1) * MAX_MSUS_PER_THREAD;
    struct dedos_thread *worker_thread = &all_threads[all_threads_index];
    struct msu_stats_data *main_msu_stats_data = main_thread->thread_stats->msu_stats_data;

    mutex_lock(worker_thread->thread_stats->mutex);
    memcpy(&main_msu_stats_data[cpy_start_index],
           worker_thread->thread_stats->msu_stats_data,
           sizeof(struct msu_stats_data) * MAX_MSUS_PER_THREAD);

    main_thread->thread_stats->num_msus += worker_thread->thread_stats->num_msus;
    mutex_unlock(worker_thread->thread_stats->mutex);

    return 0;
}

void print_aggregate_stats(void){
    struct msu_stats_data* main_stats = main_thread->thread_stats->msu_stats_data;
    log_stats("Total entries (including free space): %u", main_thread->thread_stats->array_len);
    log_stats("Total actual msus: %u", main_thread->thread_stats->num_msus);
    iterate_print_thread_stats_array(main_thread->thread_stats);
}
