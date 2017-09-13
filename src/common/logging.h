#ifndef LOGGING_H_
#define LOGGING_H_

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_PURPLE   "\x1b[35m"

#define PICO_SUPPORT_NDEBUG

#define LOG_FD stderr

#ifndef LOG_ALL
#define LOG_ALL 0
#else
#undef LOG_ALL
#define LOG_ALL 1
#endif

#define log_at_level(lvl_label, color, fd, fmt, ...)\
        fprintf(fd, "" color "%lu:%s:%d:%s(): " lvl_label ": " fmt ANSI_COLOR_RESET "\n", \
                pthread_self(), __FILE__, __LINE__, __func__, ##__VA_ARGS__)


#ifdef LOG_DEBUG
#define log_debug(fmt, ...)\
    log_at_level("DEBUG", ANSI_COLOR_RESET, LOG_FD, fmt, ##__VA_ARGS__)
#else
#define log_debug(...) do {} while (0)
#endif
#define debug(...) log_debug(__VA_ARGS__)

#ifdef LOG_INFO
#define log_info(fmt, ...)\
    log_at_level("INFO", ANSI_COLOR_GREEN, LOG_FD, fmt, ##__VA_ARGS__)
#else
#define log_info(...) do {} while (0)
#endif


#ifdef LOG_ERROR
#define log_error(fmt, ...)\
    log_at_level("ERROR", ANSI_COLOR_RED, LOG_FD, fmt, ##__VA_ARGS__)
#define log_perror(fmt, ...)\
    log_at_level("ERROR", ANSI_COLOR_RED, LOG_FD, fmt ": %s", ##__VA_ARGS__, strerror(errno))
#else
#define log_error(...) do {} while (0)
#define log_perror(...) do {} while (0)
#endif

#ifdef LOG_WARN
#define log_warn(fmt, ...)\
    log_at_level("WARN", ANSI_COLOR_YELLOW, LOG_FD, fmt, ##__VA_ARGS__)
#else
#define log_warn(...) do {} while (0)
#endif

#if defined(LOG_CRITICAL) && LOG_CRITICAL
#define log_critical(fmt, ...)\
    log_at_level("CRITICAL", ANSI_COLOR_PURPLE, LOG_FD, fmt, ##__VA_ARGS__)
#else
#define log_critical(...) do {} while (0)
#endif

#ifdef LOG_CUSTOM
#define LOG_CUSTOM_STRINGIFY(val) "" #val
#define log_custom(level, fmt, ...)\
    do { \
        if (LOG_ALL || strcmp( "" #level, LOG_CUSTOM_STRINGIFY(level))) { \
            log_at_level(#level, ANSI_COLOR_PURPLE, LOG_FD, fmt, ##__VA_ARGS__); \
        } \
    } while (0)
#else
#define log_custom(...) do {} while (0)
#endif

#if defined(DATA_PROFILING_PRINT) && DATA_PROFILING_PRINT
#define log_profile(fmt, ...) \
    log_at_level("DATA_PROFILE", ANSI_COLOR_RESET, LOG_FD, fmt, ##__VA_ARGS__)
#else
#define log_profile(...) do {} while (0)
#endif

#if defined(LOG_TCP_DBG) && LOG_TCP_DBG
#define tcp_dbg(fmt, ...) \
    log_at_level("TCP_DEBUG", ANSI_COLOR_RESET, LOG_FD, fmt, ##__VA_ARGS__)
#else
#define tcp_dbg(...) do {} while (0)
#endif

#endif
