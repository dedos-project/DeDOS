#ifndef LOGGING_H_
#define LOGGING_H_

#include <stdio.h>
#define DEBUG 1

#define debug(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); \
                              fprintf(stderr, "\n"); } while (0)

#endif
