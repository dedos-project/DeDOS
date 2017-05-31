#define _GNU_SOURCE
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "stats.h"
#include "communication.h"
#include "runtime.h"
#include "control_protocol.h"
#include "dedos_msu_list.h"
#include "logging.h"
#include "dedos_thread_queue.h"
#include "dedos_msu_msg_type.h"
#include "generic_msu_queue.h"
#include "generic_msu_queue_item.h"
#include "msu_tracker.h"
#include "control_operations.h"
#include "global.h"

#define REQUEST_MSG_SIZE 64


static void print_all_threads(){
    int i = 0;
    struct dedos_thread *cur;
    for(i = 0; i < total_threads; i++){
        cur = &all_threads[i];
        if(cur->tid > 0){
            log_debug("tid: %lu, behavior: %s", cur->tid, cur->thread_behavior == 1 ? "blocking" : "non-blocking");
        }
    }
}

void request_init_config(void) {
    struct dedos_control_msg req;
    req.msg_type = REQUEST_MESSAGE;
    req.msg_code = GET_INIT_CONFIG;
    req.payload_len = numCPU;
    req.payload = NULL;

    dedos_send_to_master((char*) &req, sizeof(req));
}

int get_thread_index(pthread_t tid) {
//  pthread_t self_id = pthread_self();
    int i;
    mutex_lock(all_threads_mutex);
    for (i = 0; i < MAX_THREADS; i++) {
        // log_debug("Candidate tid: %lu", all_threads[i].tid);
        if (pthread_equal(all_threads[i].tid, tid)) {
            mutex_unlock(all_threads_mutex);
            // log_debug("Found index : %d",i);
            return i;
        }
    }
    mutex_unlock(all_threads_mutex);
    return -1;
}

struct dedos_thread* get_ptr_to_self(void) {
    pthread_t self_id = pthread_self();
    struct dedos_thread *res;
    int i;

    /* since the main thread might be updating the all_threads structure
     this will rarely be a botleneck as the main will only change this
     struct when add/removing blocking msus that run on their own
     thread
     */
    mutex_lock(all_threads_mutex);
    for (i = 0; i < total_threads; i++) {
        if (pthread_equal(all_threads[i].tid, self_id)) {
            res = &all_threads[i];
            mutex_unlock(all_threads_mutex);
            return res;
        }
    }
    mutex_unlock(all_threads_mutex);
    return NULL;
}

static int print_thread_affinity() {
    int s, j;
    cpu_set_t cpuset;

    /* Check the actual affinity mask assigned to the thread */
    s = pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (s != 0) {
        log_error("pthread_getaffinity_np failed: %s", strerror(errno));
    }
    log_debug("Set returned by pthread_getaffinity_np() contained: %s", "");
    for (j = 0; j < 8; j++)
        if (CPU_ISSET(j, &cpuset))
            printf("\t\tCPU %d\n", j);
    return 0;
}

static int pin_thread(pthread_t ptid, int cpu_id) {
    int s, j;
    cpu_set_t cpuset;

    /* Set affinity mask to include CPUs i*/
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    s = pthread_setaffinity_np(ptid, sizeof(cpu_set_t), &cpuset);
    if (s != 0) {
        log_error("pthread_setaffinity_np failed: %s", strerror(errno));
    }
    /* Check the actual affinity mask assigned to the thread */
    s = pthread_getaffinity_np(ptid, sizeof(cpu_set_t), &cpuset);
    if (s != 0) {
        log_error("pthread_getaffinity_np failed: %s", strerror(errno));
    }
    /*
     debug("DEBUG: Set returned by pthread_getaffinity_np() contained: %s", "");
     for (j = 0; j < 8; j++)
     if (CPU_ISSET(j, &cpuset))
     printf("\t\tCPU %d\n", j);
     */

    return 0;
}

static inline void update_thread_stats(struct dedos_thread* thread, struct generic_msu *msu){
    //find MSU stats entry in thread struct
    struct index_tracker* index_tracker;
    index_tracker = find_index_tracking_entry(thread->thread_stats->msu_stats_index_tracker, msu->id);
    if(!index_tracker){
        log_error("Cannot update stats for msu id: %d", msu->id);
        return;
    }
    struct msu_stats_data *msu_stats_data = index_tracker->stats_data_ptr;
    mutex_lock(thread->thread_stats->mutex);
    // inside a lock because main thread might read it intermittently to update systemwide statistics
    if (msu_stats_data->msu_id != msu->id) {
        log_error("MSU id mismatch for updating stats from msu to thread struct, id: %d", msu->id);
        return;
    } else {
        // copy msu statistics into thread's statistics collector struct,
        msu_stats_data->queue_item_processed[1] = msu->stats.queue_item_processed[1];
        msu_stats_data->queue_item_processed[0] = msu->stats.queue_item_processed[0];
        msu_stats_data->memory_allocated[1] = msu->stats.memory_allocated[1];
        msu_stats_data->memory_allocated[0] = msu->stats.memory_allocated[0];
        msu_stats_data->data_queue_size[1] = msu->q_in.size;
        msu_stats_data->data_queue_size[0] = time(NULL);
        /* Print stats */
#if DEBUG != 0
//        iterate_print_thread_stats_array(thread->thread_stats);
#endif
    }
    mutex_unlock(thread->thread_stats->mutex);
}

static void* non_block_per_thread_loop() {
    /* Each of these loops is independently run on thread */
    struct dedos_thread* self;
    int thread_index;
    usleep(1);
    thread_index = get_thread_index(pthread_self());
    log_debug("Index in all thread: %d", thread_index);
    self = &all_threads[thread_index];
    int not_done = 1;
    do {
        if(!pthread_equal(self->tid, pthread_self())){
            log_error("Unable to get correct pointer to self thread struct %s","");
        }
        else{
            not_done = 0;
        }
    } while (not_done);

    usleep(1);
    log_info("Starting nonblocking per thread loop: %lu", self->tid);

    log_debug("-------------------------%s", "");
    log_debug("Thread struct addr: %p", self);
    log_debug("Thread thread_q addr: %p", self->thread_q);
    log_debug("Thread pool addr: %p", self->msu_pool);
    log_debug("-------------------------%s", "");
    struct rusage thread_usage;

    while (1) {

        int rtn  = sem_wait(self->q_sem);
        if (rtn < 0){
            log_error("Error waiting on thread queue semaphore");
        }

#if STATLOG
        log_warn("STAT_LOG Enabled");
        getrusage(RUSAGE_THREAD, &thread_usage);
        periodic_aggregate_stat(N_SWAPS, thread_index,
                                thread_usage.ru_nivcsw,
                                250);
        struct timespec t;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
        periodic_aggregate_stat(CPUTIME, thread_index,
                                (double)t.tv_sec + (double)t.tv_nsec/1000000000.0,
                                200);

        periodic_aggregate_end_time(THREAD_LOOP_TIME,
                                    thread_index,
                                    100);
        aggregate_start_time(THREAD_LOOP_TIME, thread_index);

#endif
        /* 1. MSU processing */
        //RR over each msu in MSU pool
#ifdef PICO_SUPPORT_TIMINGS
        // START_TIME;
#endif
        struct msu_pool *pool = self->msu_pool;
        struct generic_msu *cur = pool->head;
        struct generic_msu *end = pool->tail;

        if (cur != NULL) {
            do {
                unsigned int covered_weight = 0;
                struct generic_msu_queue_item *queue_item;

                while(covered_weight < cur->scheduling_weight){
                    //dequeue from data queue
                    queue_item = generic_msu_queue_dequeue(&cur->q_in);
                    if (queue_item) {

                        aggregate_stat(QUEUE_LEN, cur->id, cur->q_in.num_msgs, 1);
                        aggregate_end_time(MSU_INTERIM_TIME, cur->id);
                        debug("DEBUG: Thread %02x dequeuing MSU %d data queue", self->tid, cur->id);

                        aggregate_start_time(MSU_FULL_TIME, cur->id);
                        /* NOTE: data_handler should consume the item, i.e. free the memory
                         by calling delete_generic_msu_queue_item(queue_item) */
                        msu_receive(cur, queue_item);
                        aggregate_end_time(MSU_FULL_TIME, cur->id);

                        //increment queue item processed
                        cur->stats.queue_item_processed[1]++; //FIXME UINT OVERFLOW
                        cur->stats.queue_item_processed[0] = time(NULL);

                        aggregate_stat(ITEMS_PROCESSED, cur->id, cur->stats.queue_item_processed[1], 0);

                        debug("DEBUG: msu %d has processed MD %d items at time %d",
                              cur->id, cur->stats.queue_item_processed[1],
                              cur->stats.queue_item_processed[0]);

                        aggregate_start_time(MSU_INTERIM_TIME, cur->id);
                    } else if (cur->type->type_id == DEDOS_TCP_DATA_MSU_ID) {
                        //To make sure stack ticks even if there was no item
                        msu_receive(cur, NULL);
                    } else if (cur->type->type_id == DEDOS_TCP_HANDSHAKE_MSU_ID) {
                        //To make sure internal state is cleared
                        msu_receive(cur, NULL);
                    } else if (cur->type->type_id == DEDOS_BAREMETAL_MSU_ID) {
                        //To make sure internal state is cleared
                        msu_receive(cur, NULL);
                    } else if (cur->type->type_id == DEDOS_SOCKET_HANDLER_MSU_ID) {
                        log_debug("%s","triggering socket handler receive");
                        //To poll socket
                        msu_receive(cur, NULL);
                    }
                    covered_weight++;
                }

                //dequeue from control queue
                if (&cur->q_control) {
                    queue_item = generic_msu_queue_dequeue(&cur->q_control);
                    if (queue_item) {
                        /* NOTE: control_update_handler should consume the item, i.e. free the memory
                         of all pointers inside the queue_item and the queue_item itself */
                        msu_receive_ctrl(cur, queue_item);
                    }
                }

                update_thread_stats(self, cur);
                cur = cur->next;

            } while (cur);
        }
        /* 2. Control updates */
        //TODO add counter to call in intervals
        if (!self->thread_q) {
            log_error("Missing thread_q! %s","");
        } else {// else if (self->thread_q->size || self->thread_q->num_msgs) {
            // log_debug("Number of messages in thread queue: %u", self->thread_q->num_msgs);
            // log_debug("Total thread queue size: %u", self->thread_q->size);
            process_control_updates();
            // log_info("Done processing control updates..%s","");
        }
    }

    log_debug("Leaving thread %u", pthread_self());
    free(self->thread_q);
    return NULL;
}

static int create_thread(int id) {
    int err;
    err = pthread_create(&(all_threads[id].tid), NULL,
            &non_block_per_thread_loop, NULL);
    if (err != 0) {
        log_error("Can't create thread :[%s]", strerror(err));
        return -1;
    }
    return 0;
}

static int destroy_pthread(pthread_t thread){
    int s = -1;
    s = pthread_cancel(thread);
    if (s != 0){
        log_error("Failed to cancel pthread cancel%s","");
        return -1;
    }
    log_debug("Destroyed thread %lu",thread);
    return 0;
}

int on_demand_create_worker_thread(int is_blocking) {

    log_debug("On demand worker thread creation, blocking = %d", is_blocking);
    int index = -1;
    int ret;
    int new_total_threads;
    new_total_threads = total_threads + 1;
    struct dedos_thread *cur_thread;

    mutex_lock(all_threads_mutex);
    cur_thread = &all_threads[total_threads];
    mutex_unlock(all_threads_mutex);

    log_debug("all thread start: %p",all_threads);
    log_debug("cur_thread: %p",cur_thread);
    index = total_threads;

    //initialize dedos_thread struct member variables
    all_threads[index].msu_pool = NULL;
    all_threads[index].msu_pool = (struct msu_pool*) malloc(sizeof(struct msu_pool));
    if (!(all_threads[index].msu_pool)) {
        log_error("%s", "Unable to allocate memory for MSU pool");
        return -1; //FIXME tell global controller of failure
    }

    all_threads[index].q_sem = malloc(sizeof(sem_t));
    if (!(all_threads[index].q_sem)){
        log_error("Unable to allocate thread queue semaphore");
        free(all_threads[index].msu_pool);
        return -1;
    }
    ret = sem_init(all_threads[index].q_sem, 0, 0);
    if (ret < 0){
        log_error("Unable to initialize queue semaphore");
        free(all_threads[index].msu_pool);
        free(all_threads[index].q_sem);
        return -1;
    }


    // cur_thread->msu_pool = msu_pool;
    // cur_thread->msu_pool->num_msus = 0;
    // cur_thread->msu_pool->mutex = NULL;
    // cur_thread->msu_pool->head = NULL;
    // cur_thread->msu_pool->tail = NULL;

    all_threads[index].msu_pool->num_msus = 0;
    all_threads[index].msu_pool->mutex = NULL;
    all_threads[index].msu_pool->head = NULL;
    all_threads[index].msu_pool->tail = NULL;

    cur_thread->thread_stats = (struct thread_msu_stats*) malloc(sizeof(struct thread_msu_stats));
    if (!(cur_thread->thread_stats)) {
        log_error("%s", "Unable to allocate memory for thread_stats");
        return -1; //FIXME tell global controller of failure
    }

    ret = init_thread_stats(cur_thread->thread_stats);
    if(ret){
        free(cur_thread->thread_stats);
        return -1;
    }

    cur_thread->thread_q = (struct dedos_thread_queue*) malloc(sizeof(struct dedos_thread_queue));
    if (!(cur_thread->thread_q)) {
        log_error("%s", "Unable to allocate memory for thread queue");
        free(cur_thread->msu_pool);
        return -1;
    }
    /* since msgs will be enqueued by socket I/O thread and
     * dequeued by the thread itself in its while loop
     */
    cur_thread->thread_q->shared = 1;
    cur_thread->thread_q->max_size = MAX_THREAD_Q_SIZE;
    cur_thread->thread_q->max_msgs = MAX_THREAD_Q_MSGS;
    cur_thread->thread_q->size = 0;
    cur_thread->thread_q->num_msgs = 0;
    cur_thread->thread_q->head = NULL;
    cur_thread->thread_q->tail = NULL;
    cur_thread->thread_q->mutex = NULL;

    //actually create thread
    mutex_lock(all_threads_mutex);
    ret = create_thread(index);
    if (ret == -1) {
        log_error("Failure %s","");
        return -1;
    }
    mutex_unlock(all_threads_mutex);

    if (is_blocking) {
        cur_thread->thread_behavior = BLOCKING_THREAD;
    } else {
        cur_thread->thread_behavior = NON_BLOCKING_THREAD;
        //meaning it's a worker thread to be pinned
        //find the first unused core
        int l;
        for(l = 0; l < numCPU; l++){
            if(used_cores[l] == 0){
                break;
            }
        }
        ret = pin_thread(cur_thread->tid, l);
        used_cores[l] = 1;
        log_debug("Pinned non-blocking thread to core: %u",l);
    }

    log_debug("Success creating a new thread with tid %02x, placed at index: %d, blocking: %d",
              cur_thread->tid, index, is_blocking);

    log_debug("-------------------------%s", "");
    log_debug("Thread struct addr: %p",cur_thread);
    log_debug("Thread thread_q addr: %p", cur_thread->thread_q);
    log_debug("Thread pool addr: %p", cur_thread->msu_pool);
    log_debug("-------------------------%s", "");

    //update the tracker variable
    total_threads = new_total_threads;

    //update main threads aggregate stat tracker array size
    ret = update_aggregate_stats_size(main_thread->thread_stats, total_threads - 1); //(total_threads - 1) because only worker threads
    if(ret == -1){
        log_error("Failed to update the size of aggregate stats for main thread %s","");
    }
    log_info("Total threads: %u", total_threads);

#if INFO != 0
    print_all_threads();
#endif

    return index;
}

int destroy_worker_thread(struct dedos_thread *dedos_thread){
    //destroy's thread's internal stuff like thread queue, all the msu's etc...
    //empty msu_pool
    log_debug("Started destroying worker thread %lu",dedos_thread->tid);

    dedos_msu_pool_destroy(dedos_thread->msu_pool);
    log_debug("Destroyed msu pool for thread%s","");
    //empty & free thread_queue
    dedos_thread_queue_empty(dedos_thread->thread_q);
    //empty & free thread_stats
    destroy_thread_stats(dedos_thread->thread_stats);

    free(dedos_thread->thread_q);
    destroy_pthread(dedos_thread->tid);
    return 0;
}

int init_main_thread(void) {
    /* create threads = number of cores and associate a queue with each thread */
    int ret;
    //Get number number of cores
    int i = 0, j = 0;
    numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    log_debug("Num of cores : %d", numCPU);
    total_threads = 1;
    all_threads = (struct dedos_thread*) malloc(MAX_THREADS * sizeof(struct dedos_thread));
    if(!all_threads){
        log_error("Failed to malloc all_threads %s","");
        return -1;
    }
    memset(all_threads, '\0', sizeof(struct dedos_thread) * MAX_THREADS);

    used_cores = (unsigned int *)malloc(numCPU * sizeof(unsigned int));
    if(!used_cores){
        log_error("Failed to malloc used_cores%s","");
        return -1;
    }
    for(i = 0; i < numCPU; i++){
        used_cores[i] = 0;
    }
    //main thread is not pinned since its blocking socket I/O
    main_thread = (struct dedos_thread*) malloc(sizeof(struct dedos_thread));
    main_thread->tid = pthread_self();
    main_thread->thread_behavior = BLOCKING_THREAD;
    main_thread->thread_q = (struct dedos_thread_queue*) malloc(
            sizeof(struct dedos_thread_queue));
    if(!main_thread->thread_q){
        log_error("Failed to malloc thread queue for main thread %s","");
        return -1;
    }
    main_thread->msu_pool = NULL; //Only sockets with select so no MSUs
    main_thread->thread_q->shared = 1;
    main_thread->thread_q->mutex = mutex_init();
    main_thread->thread_q->num_msgs = 0;
    main_thread->thread_q->max_msgs = 0;
    main_thread->thread_q->max_size = 0;
    main_thread->thread_q->head = NULL;
    main_thread->thread_q->tail = NULL;
    main_thread->q_sem = NULL;

    //initially there are no worker threads, so not stats to keep
    main_thread->thread_stats = (struct thread_msu_stats*) malloc(sizeof(struct thread_msu_stats));
    if (!main_thread->thread_stats) {
        log_error("%s", "Unable to allocate memory for thread_stats");
        return -1; //FIXME tell global controller of failure
    }
    ret = init_aggregate_thread_stats(main_thread->thread_stats, 0);
    if(ret){
        log_error("Unable to malloc thread stats for main thread %s","");
        free(main_thread->thread_stats);
        return -1;
    }

    log_debug("Main thread struct init %s", "");

    return 0;
}

static int destroy_main_thread(void){
    free(used_cores);
    mutex_destroy(main_thread->thread_q->mutex);
    free(main_thread->thread_q);
    log_debug("Destroyed main thread%s","");
    return 0;
}

int create_worker_threads(void) {
    /* create threads = number of cores and associate a queue with each thread */
    int ret;
    //Get number number of cores
    unsigned int i = 0;
    int j = 0;
    numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    log_debug("Num of cores : %d", numCPU);

    i = 0;
    while (i < total_threads && j < numCPU) {
        ret = create_thread(i);
        all_threads[i].thread_behavior = NON_BLOCKING_THREAD;
        all_threads[i].msu_pool = (struct msu_pool*) malloc(
                sizeof(struct msu_pool));
        if (!(all_threads[i].msu_pool)) {
            log_error("%s", "Unable to allocate memory for MSU pool");
            exit(1);
        }
        all_threads[i].thread_q = (struct dedos_thread_queue*) malloc(
                sizeof(struct dedos_thread_queue));
        if (!(all_threads[i].thread_q)) {
            log_error("%s", "Unable to allocate memory for thread queue");
            free(all_threads);
            return -1;
        }
        /* since msgs will be enqueued by socket I/O thread and
         * dequeued by the thread itself in its while loop
         */
        all_threads[i].thread_q->shared = 1;
        all_threads[i].thread_q->max_size = MAX_THREAD_Q_SIZE;
        all_threads[i].thread_q->max_msgs = MAX_THREAD_Q_MSGS;

        ret = pin_thread(all_threads[i].tid, j);
        i++;
        j++;
    }
    log_info("Total worker threads (excluding main): %d", total_threads);
    return 0;
}

static void check_queue_for_data() {

    struct dedos_thread_msg *thread_msg;
    struct dedos_intermsu_message *intermsu_msg;
    char* intermsu_payload;
    int rounds = 2;

    while (rounds) {
        if (main_thread->thread_q) {
            thread_msg = dedos_thread_dequeue(main_thread->thread_q);
            if (thread_msg) {
                log_debug("%s Found a message in main thread queue!", "");
                //now based of action do stuff, CREATE_MSU, DESTROY_MSU shouldn't happen here
                //since they are taken care in check_comm_sockets.
                //This is only for data generated by MSU on runtimes that needs to be sent to other
                //runtimes, TODO could be statistics in future to be send to master
                if (thread_msg->action == FORWARD_DATA) {
                    log_debug("FORWARD_DATA action message: %s", "");
                    intermsu_msg = thread_msg->data;
                    int dst_runtime_ip = thread_msg->action_data;
                    print_dedos_intermsu_message(intermsu_msg);

                    char ip[30];
                    memset(ip, 0, 30);
                    ipv4_to_string(ip, dst_runtime_ip);
                    log_debug("Dst runtime IP to send FORWARD to: %s", ip);

                    if (intermsu_msg->payload && intermsu_msg->payload_len) {
                        intermsu_payload = intermsu_msg->payload;
                        //need to find the msu_endpoint in routing table, send function needs to know the ip of dst runtime
                        dedos_control_send(intermsu_msg, dst_runtime_ip,
                                intermsu_payload, intermsu_msg->payload_len);
                        free(intermsu_payload);
                    }
                }

                else if (thread_msg->action == FAIL_CREATE_MSU) {
                    log_debug("FAIL_CREATE_MSU message: %s", "");
                    struct msu_placement_tracker *item;
                    item = msu_tracker_find(thread_msg->action_data);
                    if (!item) {
                        log_error(
                                "Dont have track of MSU to delete, msu_id: %d",
                                thread_msg->action_data);
                        goto end;
                    }
                    msu_tracker_delete(item);
                    log_debug("Removed MSU that was not created from tracker, MSU ID: %d",
                            thread_msg->action_data);
                    msu_tracker_print_all();
                } else {
                    log_error("%s", "Unknown dedos_thread_msg action type");
                }
                end: dedos_thread_msg_free(thread_msg); //frees its payload as well
            }
        } //if main_thread->thread_q
        rounds--;
    }
}

static void check_pending_runtimes() {

    unsigned int i;
    int ret;
    uint32_t *cur_ip;
    int remaining_count = -1;

    if (pending_runtime_peers && pending_runtime_peers->count > 0) {
        remaining_count = pending_runtime_peers->count;
        //log_debug("Pending runtimes to connect to : %d", remaining_count);
        for (i = 0; i < pending_runtime_peers->count; i++) {
            cur_ip = &pending_runtime_peers->ips[i];
            char ip_buf[40];
            ipv4_to_string(&ip_buf, *cur_ip);
            //log_debug("Current IP to connect to: %s %d", ip_buf, *cur_ip);
            //Only connect if not already connected to, runtime being identified by its IP
            if (*cur_ip != 0 && !is_connected_to_runtime(cur_ip)) {
                log_debug("Found unconnected runtime IP: %s:%d", ip_buf, runtime_listener_port);
                ret = connect_to_runtime(&ip_buf, runtime_listener_port);
                //zero out the entry in pending_runtime_peers, do not decrement count
                if (ret != 0){
                    log_warn("Could not connect to runtime with ip %s:%d", ip_buf, runtime_listener_port);
                }
                pending_runtime_peers->ips[i] = 0;
                remaining_count--;
            }
        }
    }
    if (remaining_count == 0) {
        free(pending_runtime_peers->ips);
        pending_runtime_peers->count = 0;
    }
}

static void push_stats_to_controller() {

    struct dedos_control_msg *stats_msg;
    unsigned int payload_size = main_thread->thread_stats->array_len * sizeof(struct msu_stats_data);

    stats_msg = malloc(sizeof(struct dedos_control_msg));
    if (!stats_msg) {
        log_error("failed to allocate memory for sending MSU_LIST to master %s","");
        return;
    }
    stats_msg->msg_type = ACTION_MESSAGE;
    stats_msg->msg_code = STATISTICS_UPDATE;
    stats_msg->header_len = sizeof(struct dedos_control_msg);
    stats_msg->payload_len = payload_size;

    char *sendbuf = malloc(sizeof(struct dedos_control_msg) + payload_size);
    if (!sendbuf) {
        log_error(
                "ERROR: failed to allocate memory for buf to send MSU_LIST to master %s",
                "");
        free(stats_msg);
        return;
    }
    memcpy(sendbuf, stats_msg, sizeof(struct dedos_control_msg));
    memcpy(sendbuf + sizeof(struct dedos_control_msg), main_thread->thread_stats->msu_stats_data, payload_size);

#if DEBUG != 0
//    print_aggregate_stats(); //UNCOMMENT To see your screen flooded with statistics being sent
#endif

    dedos_send_to_master(sendbuf,
            sizeof(struct dedos_control_msg) + payload_size);

    free(sendbuf);
    free(stats_msg);
}

int dedos_runtime_destroy(void){

    //TODO Cleanup all threads -> all msus of that thread -> all internal MSU stuff
    log_critical("TODO Destroying runtime %s","");
    log_critical("Just exiting for now %s","");
#ifdef DATAPLANE_PROFILING
    deinit_data_plane_profiling();
#endif
/*
    int i;
    for(i = total_threads-1; i >=1; i--){
        destroy_worker_thread(&all_threads[i]);
    }

    msu_tracker_destroy();

    destroy_main_thread();
    free(all_threads);
*/
    return 0;
}

void dedos_main_thread_loop(struct dfg_config *dfg, int runtime_id) {
    int ret;
    static int next_stat_thread = 0;
    clock_t begin;
    double time_spent;

    /* all the blocking networking I/O sockets are checked here */
    log_debug("%s", "Starting socket I/O main thread loop");

    ret = init_main_thread();
    if (ret) {
        log_error("%s", "Fialed to init main thread..Exiting..!");
        exit(1);
    }

    // Add all of the known MSU types
    register_known_msu_types();

    // Start the MSU tracker
    if ( init_msu_tracker() < 0 ){
        log_critical("Could not initialize MSU tracker... Exiting.");
        exit(1);
    }


    request_init_config();
    log_info("%s", "Requested init config...");

    if (dfg != NULL){
        int rtn = implement_dfg(dfg, runtime_id);
        if (rtn < 0){
            log_error("Could not implement DFG in local runtime");
        }
    }

    begin = clock();

    while (1) {
        flush_all_stats_to_log(0);
        ret = check_comm_sockets(); //for incoming data processing
        // log_debug("Done check_comm_sockets %s","");
        if (ret == -1) {
            log_warn("Breaking from dedos_main_thread_loop%s","");
            break;
        }

        // debug("Checking main queue for data %s","");
        check_queue_for_data(); //main thread queue to see if data needs to be sent out

        // debug("DEBUG: Checking pending runtimes %s","");
        check_pending_runtimes();

        // TODO policy as to when we pull stats from worker threads

        time_spent = (double) (clock() - begin) / CLOCKS_PER_SEC;
        if (total_threads > 1 && time_spent > 5.0) {
            if (next_stat_thread != 0) { //main thread not welcome
                aggregate_start_time(GATHER_THREAD_STATS, next_stat_thread);
                ret = copy_stats_from_worker(next_stat_thread);
                aggregate_end_time(GATHER_THREAD_STATS, next_stat_thread);
                if (ret == -1) {
                    log_error("Failed to copy stats from worker thread at index %d", next_stat_thread);
                }
            } else {
                //send before a reset but not all resets
                if (main_thread->thread_stats->num_msus > 0) {
                    push_stats_to_controller();
                }
                main_thread->thread_stats->num_msus = 0;
            }
            next_stat_thread = (next_stat_thread + 1) % total_threads;
            begin = clock();
        }

    }

    dedos_runtime_destroy();
}

