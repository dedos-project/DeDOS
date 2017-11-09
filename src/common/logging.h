/**
 * @file logging.h
 *
 * Logging of status messages to the terminal.
 *
 * The DeDOS logger supports five built-in log levels
 * (debug, info, error, warn, and critical),
 * in addition to custom log levels.
 *
 * To enable a custom log level, simply write:
 * `log(LOG_SOMETHING, "Stuff to %s", log);`
 *
 * If the macro `LOG_SOMETHING` is defined, "Stuff to log" will be output.
 * Otherwise, it will be compiled away (if optimization > 0).
 */
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

/** Where logs are printed to */
#define LOG_FD stderr

// Switch LOG_ALL definition to a 1 or 0. Makes for easier checking of the condition
// in log()
#ifndef LOG_ALL
#define LOG_ALL 0
#else
#undef LOG_ALL
#define LOG_ALL 1
#endif

/** Macro utilized by all loggers **/
#define log_at_level(lvl_label, color, fd, fmt, ...)\
        fprintf(fd, "" color "%lu:%s:%d:%s(): " lvl_label ": " fmt ANSI_COLOR_RESET "\n", \
                pthread_self(), __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#ifndef log_debug
#ifdef LOG_DEBUG
/** Use of log_debug(fmt, ...) is not recommended. Use log(LVL, fmt, ...) below */
#define log_debug(fmt, ...)\
    log_at_level("DEBUG", ANSI_COLOR_RESET, LOG_FD, fmt, ##__VA_ARGS__)
#else
/** Use of log_debug(fmt, ...) is not recommended. Use log(LVL, fmt, ...) below */
#define log_debug(...)
#endif
#define debug(fmt, ...) log_debug(__VA_ARGS__)
#endif

#ifndef log_info
#ifdef LOG_INFO
#ifndef log_info
#define log_info(fmt, ...)\
    log_at_level("INFO", ANSI_COLOR_GREEN, LOG_FD, fmt, ##__VA_ARGS__)
#endif
#else
#define log_info(fmt, ...)
#endif
#endif

#ifndef log_error
#ifdef LOG_ERROR
#ifndef log_error
#define log_error(fmt, ...)\
    log_at_level("ERROR", ANSI_COLOR_RED, LOG_FD, fmt, ##__VA_ARGS__)
#endif
#define log_perror(fmt, ...)\
    log_at_level("ERROR", ANSI_COLOR_RED, LOG_FD, fmt ": %s", ##__VA_ARGS__, strerror(errno))
#else
#define log_error(fmt, ...)
#define log_perror(fmt, ...)
#endif
#endif

#ifndef log_warn
#ifdef LOG_WARN
#ifndef log_warn
#define log_warn(fmt, ...)\
    log_at_level("WARN", ANSI_COLOR_YELLOW, LOG_FD, fmt, ##__VA_ARGS__)
#endif
#else
#define log_warn(fmt, ...)
#endif
#endif

#ifndef log_critical
#if defined(LOG_CRITICAL) && LOG_CRITICAL
#ifndef log_critical
#define log_critical(fmt, ...)\
    log_at_level("CRITICAL", ANSI_COLOR_PURPLE, LOG_FD, fmt, ##__VA_ARGS__)
#endif
#else
#define log_critical(fmt, ...)
#endif
#endif

#ifndef log
#ifdef LOG_CUSTOM
/** Helper macros to test for the existence of a preprocessor definition */
#define LOG_CUSTOM_STRINGIFY(val) "" #val
#define LOG_CUSTOM_STRINGIFY2(val) LOG_CUSTOM_STRINGIFY(val)
/** Log at a custom level. Compiled away if LOG_CUSTOM is not defined,
 * and optimized out if the preprocessor macro `level` is not defined
 * (and optimization level is sufficiently high) */
#define log(level, fmt, ...)\
    do { \
        if ((LOG_ALL || strcmp( "" #level, LOG_CUSTOM_STRINGIFY(level))) \
                        && !strcmp( "NO_" #level, LOG_CUSTOM_STRINGIFY2(NO_##level))) { \
            log_at_level(#level, ANSI_COLOR_BLUE, LOG_FD, fmt, ##__VA_ARGS__); \
        } \
    } while (0)
#else
/** Log at a custom level. Compiled away if LOG_CUSTOM is not defined,
 * and optimized out if the preprocessor macro `level` is not defined
 * (and optimization level is sufficiently high) */
#define log(level, fmt, ...)
#endif
#endif

#ifndef log_profile
#if defined(DATA_PROFILING_PRINT) && DATA_PROFILING_PRINT
#define log_profile(fmt, ...) \
    log_at_level("DATA_PROFILE", ANSI_COLOR_RESET, LOG_FD, fmt, ##__VA_ARGS__)
#else
#define log_profile(...)
#endif
#endif

#ifndef tcp_dbg
#if defined(LOG_TCP_DBG) && LOG_TCP_DBG
#define tcp_dbg(fmt, ...) \
    log_at_level("TCP_DEBUG", ANSI_COLOR_RESET, LOG_FD, fmt, ##__VA_ARGS__)
#else
#define tcp_dbg(...)
#endif
#endif

#endif
