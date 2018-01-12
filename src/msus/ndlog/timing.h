#ifndef TIMING_H_
#define TIMING_H_

#include <pthread.h>
#include <sys/time.h>

#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define BILLION 1000000000L

#define TIMING_SAMPLE 5000

#define log_time(fmt, ...) \
        do { fprintf(stderr, "" ANSI_COLOR_YELLOW  "%lu:%d:%s(): " fmt, pthread_self(), \
                                __LINE__, __func__, __VA_ARGS__); \
                              fprintf(stderr, ANSI_COLOR_RESET "\n"); } while (0)

#define START_TIME \
    static uint64_t stack_tick_time_dedos_avg = 0, diff = 0; \
    static uint64_t count = 0; \
    struct timespec tstart; \
    clock_gettime(CLOCK_REALTIME, &tstart);

#define END_TIME(x) \
    struct timespec tend; \
    clock_gettime(CLOCK_REALTIME, &tend); \
    diff = BILLION * (tend.tv_sec - tstart.tv_sec) + tend.tv_nsec - tstart.tv_nsec; \
    stack_tick_time_dedos_avg = (stack_tick_time_dedos_avg * count) + (diff); \
    count++; \
    stack_tick_time_dedos_avg /= (count); \
    if(count % TIMING_SAMPLE == 0){ \
        log_time("%s avg for N = %llu: %llu nanosec",x, count, stack_tick_time_dedos_avg); \
        stack_tick_time_dedos_avg = 0; \
        count = 0; \
    } \

#endif /* TIMING_H_ */
