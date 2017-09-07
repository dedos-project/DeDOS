#ifndef DEDOS_TESTING_H_
#define DEDOS_TESTING_H_

#include <check.h>

#define START_DEDOS_TEST(name) \
    START_TEST(name) { \
        fprintf(LOG_FD, ANSI_COLOR_BLUE "******** " #name " ******** \n" ANSI_COLOR_RESET);

#define END_DEDOS_TEST \
    } END_TEST


#define LOG_ALL
#define LOG_CUSTOM
#include "logging.h"

#define DEDOS_START_TEST_LIST(name) \
    int main(int argc, char **argv) { \
        Suite *s = suite_create(name); \
        TCase *tc = tcase_create(name); \

#define DEDOS_ADD_TEST_FN(fn) \
        tcase_add_test(tc, fn);

#define DEDOS_END_TEST_LIST() \
        suite_add_tcase(s, tc); \
        SRunner *sr;\
        sr = srunner_create(s); \
        srunner_run_all(sr, CK_NORMAL); \
        int num_failed = srunner_ntests_failed(sr); \
        srunner_free(sr); \
        return (num_failed == 0) ? 0 : -1; \
    }

#endif
