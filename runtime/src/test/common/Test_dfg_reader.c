#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "dfg_reader.c"

struct dedos_dfg *setup_dfg(int total_threads, int pinned_threads, int marked_pinned, int marked_unpinned) {
    struct dedos_dfg *dfg = calloc(1, sizeof(*dfg));
    struct dfg_runtime *rt = calloc(1, sizeof(*rt));
    struct dfg_thread *threads = calloc(total_threads, sizeof(*threads));

    rt->n_pinned_threads = pinned_threads;
    rt->n_unpinned_threads = total_threads - pinned_threads;

    for (int i=0; i<total_threads; i++) {
        rt->threads[i] = &threads[i];
        if (i < marked_pinned) {
            rt->threads[i]->mode = PINNED_THREAD;
        } else if (i < marked_pinned + marked_unpinned) {
            rt->threads[i]->mode = UNPINNED_THREAD;
        }
    }

    dfg->runtimes[0] = rt;
    dfg->n_runtimes = 1;
    return dfg;

}

START_TEST(test_fix_num_threads_correct) {
    int total_threads= 10;
    int pinned_threads = 4;
    int unpinned_threads = total_threads- pinned_threads;
    int marked_pinned = 2;
    int marked_unpinned = 3;
    struct dedos_dfg *dfg = setup_dfg(total_threads, pinned_threads, marked_pinned, marked_unpinned);

    int rtn = fix_num_threads(dfg);

    ck_assert(rtn == 0);

    int set_pinned = 0;
    int set_unpinned = 0;
    for (int i=0; i<total_threads; i++) {
        if (dfg->runtimes[0]->threads[i]->mode == PINNED_THREAD) {
            set_pinned++;
        } else if (dfg->runtimes[0]->threads[i]->mode == UNPINNED_THREAD) {
            set_unpinned++;
        } else {
            ck_abort_msg("Thread has undefined pinned status");
        }
    }
    log_info("Set unpinned: %d", set_unpinned);
    ck_assert_int_eq(set_pinned, pinned_threads);
    ck_assert_int_eq(set_unpinned,unpinned_threads);

} END_TEST

START_TEST(test_fix_num_threads_incorrect) {
    struct dedos_dfg *dfg = setup_dfg(5, 2, 3, 1);

    int rtn = fix_num_threads(dfg);

    ck_assert(rtn != 0);
} END_TEST

int main(int argc, char **argv) {
    Suite *s  = suite_create("dfg_test");

    TCase *tc = tcase_create("num_threads");
    //tcase_add_test(tc_stats, test_sample_item_stats);
    tcase_add_test(tc, test_fix_num_threads_correct);
    tcase_add_test(tc, test_fix_num_threads_incorrect);
    suite_add_tcase(s, tc);

    SRunner *sr;

    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : -1;
}

