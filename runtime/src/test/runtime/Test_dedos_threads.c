#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "dedos_threads.c"

START_TEST(test_get_uninitialized_dedos_thread) {
   struct dedos_thread *thread = get_dedos_thread(3);
   ck_assert(thread == NULL);
} END_TEST

START_TEST(test_get_initialized_dedos_thread) {
    struct dedos_thread *thread = malloc(sizeof(*thread));
    init_dedos_thread(thread, PINNED_THREAD, 3);
    struct dedos_thread *thread_out = get_dedos_thread(3);
    ck_assert(thread == thread_out);
} END_TEST

int main(int argc, char **argv) {
    Suite *s  = suite_create("dedos_threads");

    TCase *tc = tcase_create("dedos_threads");
    log_error("TEST!");
    //tcase_add_test(tc_stats, test_sample_item_stats);
    tcase_add_test(tc, test_get_uninitialized_dedos_thread);
    tcase_add_test(tc, test_get_initialized_dedos_thread);
    suite_add_tcase(s, tc);

    SRunner *sr;

    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : -1;
}

