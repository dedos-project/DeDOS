#ifndef TESTING_UTILS_H
#define TESTING_UTILS_H

#include <libgen.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <check.h>
#include <errno.h>
#include "global.h"

#define RESOURCE_DIR "resources"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_PURPLE   "\x1b[35m"

#define _log_test(test_name, color, label, fmt, ...)\
    do { \
       fprintf(stderr, "" ANSI_COLOR_RED " %s:%s:%d:%s():" label ": " fmt, \
               test_name, __FILE__, __LINE__, __func__, ##__VA_ARGS__);\
       fprintf(stderr, "" ANSI_COLOR_RESET "\n"); } while (0)

#define log_test_error(test_name, fmt, ...)\
    _log_test(test_name, ANSI_COLOR_RED, "ERROR", fmt, ##__VA_ARGS__)

char RES_PATH[256];
char RES_ROOT[256];

char* _get_resource_path(const char* test_name, char *resource){
    mark_point();
    if (RES_ROOT == NULL){
        log_test_error(test_name, "Testing resources not initialized");
        return NULL;
    }
    sprintf(RES_PATH, "%s/%s", RES_ROOT, resource);
    int rtn = access(RES_PATH, R_OK);
    if ( rtn != -1 ) {
        return RES_PATH;
    } else {
        log_test_error(test_name, "Resource %s not found: %s", RES_PATH, strerror(errno));
        return NULL;
    }
}

#define get_resource_path(resource) _get_resource_path(__func__, resource)

SSL_CTX* ssl_ctx_global;
void testing_init(char *exec_path){
    strcpy(RES_ROOT, exec_path);
    dirname(RES_ROOT);
    sprintf(RES_ROOT, "%s/%s", RES_ROOT, RESOURCE_DIR);
}

#endif
