#ifndef LOGGING_H_
#define LOGGING_H_

#include <stdio.h>
#include <pthread.h>
#include "timing.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_PURPLE   "\x1b[35m"

#define PICO_SUPPORT_NDEBUG
#define DEBUG 1
#define INFO 0
#define ERROR 1
#define WARN 1
#define TCP_DEBUG 0
#define STATS 0
#define DATA_PROFILING 1

#define log_error(fmt, ...) \
        do { if (ERROR) { fprintf(stderr, "" ANSI_COLOR_RED "%lu:%s:%d:%s(): ERROR: " fmt, pthread_self(),__FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); \
                              fprintf(stderr,""  ANSI_COLOR_RESET "\n"); }  } while (0)

#define log_critical(fmt, ...) \
      do { if (1) { fprintf(stderr, "" ANSI_COLOR_PURPLE "%lu:%s:%d:%s(): ALARM: " fmt, pthread_self(),__FILE__, \
                              __LINE__, __func__, ##__VA_ARGS__); \
                            fprintf(stderr,""  ANSI_COLOR_RESET "\n"); }  } while (0)



#ifdef PICO_SUPPORT_NDEBUG
#define debug(fmt, ...) \
        do { if (DEBUG) { fprintf(stderr, "%lu:%s:%d:%s(): DEBUG: " fmt, pthread_self(),__FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); \
                              fprintf(stderr, "\n"); } } while (0)

#define log_debug(fmt, ...) \
        do { if (DEBUG) { fprintf(stderr, "%lu:%s:%d:%s(): DEBUG: " fmt, pthread_self(),__FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); \
                              fprintf(stderr, "\n"); } } while (0)
#define log_info(fmt, ...) \
        do { if (INFO) { fprintf(stderr, "" ANSI_COLOR_GREEN  "%lu:%s:%d:%s(): INFO: " fmt, pthread_self(),__FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); \
                              fprintf(stderr, ANSI_COLOR_RESET "\n"); } } while (0)
#define log_warn(fmt, ...) \
        do { if (WARN) { fprintf(stderr, "" ANSI_COLOR_YELLOW "%lu:%s:%d:%s(): WARN: " fmt, pthread_self(),__FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); \
                              fprintf(stderr,""  ANSI_COLOR_RESET "\n"); }  } while (0)

#ifdef STATS
#define log_stats(fmt, ...) \
        do { if (1) { fprintf(stderr,  "%lu:%s:%d:%s(): STATS: " fmt "\n", pthread_self(),__FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); \
        }} while (0)
#else
#define log_stats(...)
#endif

#ifdef DATA_PROFILING
#define log_profile(fmt, ...) \
        do { if (1) { fprintf(stderr,  "%lu:%s:%d:%s(): DATA_PROFILE: " fmt "\n", pthread_self(),__FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); \
        }} while (0)
#else
#define log_profile(...)
#endif


#define tcp_dbg(fmt, ...) \
        do { if (TCP_DEBUG) { fprintf(stderr, "%lu:%s:%d:%s(): TCP_DEBUG: " fmt, pthread_self(),__FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); \
                               } } while (0)

#else

#define debug(fmt, ...)

#define log_debug(fmt, ...)

#define log_info(fmt, ...)

#define log_warn(fmt, ...)

#define log_stats(fmt, ...)

#define tcp_dbg(fmt, ...)

#endif

#endif
