#include "dedos_testing.h"
#include "rt_stats.c"


START_DEDOS_TEST(test_sorting) {
    struct timed_stat stats[10];
    for (int i=0; i < 10; i++) {
        stats[i].value =  i * 19 % 13;
    }
    for (int i=0; i < 10; i++) {
        log_info("%d: %lf", i, (double)stats[i].value);
    }
    qsort_by_value(stats, 10);
    log_critical("AFTER");
    for (int i=0; i < 10; i++) {
        log_info("%d: %lf", i, (double)stats[i].value);
    }
    for (int i=1; i < 10; i++) {
        ck_assert_int_le(stats[i-1].value, stats[i].value);
    }
}
END_DEDOS_TEST

DEDOS_START_TEST_LIST("rt_stats")

    DEDOS_ADD_TEST_FN(test_sorting)

DEDOS_END_TEST_LIST()
