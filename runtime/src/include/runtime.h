#ifndef RUNTIME_H_
#define RUNTIME_H_

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include "routing.h"
#include "dedos_thread_queue.h"
#include "control_msg_handler.h"
#include "dedos_msu_pool.h"
#include "dedos_statistics.h"

#define MAX_THREADS 20

#define BLOCKING_THREAD 1
#define NON_BLOCKING_THREAD 2

/* each thread and some associated info */
struct dedos_thread
{
    pthread_t tid;
    uint8_t thread_behavior; //blocking or not
    struct dedos_thread_queue *thread_q; //queue for incoming control messages
    struct msu_pool *msu_pool; //pointer to struct that will keep track of msu's assigned to the thread
    struct thread_msu_stats *thread_stats;
};

//including main thread
struct dedos_thread *all_threads;
void* all_threads_mutex;

struct dedos_thread *main_thread;

int numCPU;
int total_threads;
int *used_cores;

void request_init_config(void);
int create_worker_threads(void);

struct dedos_thread* get_ptr_to_self(void);
int get_thread_index(pthread_t tid);
void dedos_main_thread_loop(void);
int on_demand_create_worker_thread(int is_blocking);
int destroy_worker_thread(struct dedos_thread *dedos_thread);
int dedos_runtime_destroy(void);
void freeSSLRelatedStuff(void);


#endif /* RUNTIME_H_ */
