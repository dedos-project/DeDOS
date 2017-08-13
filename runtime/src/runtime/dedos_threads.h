
enum thread_behavior {
    BLOCKING_THREAD = 1;
    NONBLOCK_THREAD = 2;
};

struct dedos_thread {
    pthread_t tid;
    enum thread_behavior behavior;
    struct msg_queue queue; // Queue for incoming messages
};

#define MAX_MSU_PER_THREAD 8

struct worker_thread {
    struct dedos_thread thread;
    int n_msu;
    struct local_msu *msus[MAX_MSU_PER_THREAD];
}

int init_main_thread(void); 
