#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "dfg_reader.c"

char test_dfg_path[64];

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

void tear_down_dfg(struct dedos_dfg *dfg) {
    free(dfg->runtimes[0]->threads[0]);
    free(dfg->runtimes[0]);
    free(dfg);
}

START_DEDOS_TEST(test_fix_num_threads_correct) {
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
    tear_down_dfg(dfg);
} END_DEDOS_TEST

START_DEDOS_TEST(test_fix_num_threads_incorrect) {
    struct dedos_dfg *dfg = setup_dfg(5, 2, 3, 1);

    int rtn = fix_num_threads(dfg);

    ck_assert(rtn != 0);
    tear_down_dfg(dfg);
} END_DEDOS_TEST

START_DEDOS_TEST(test_parse_dfg_json_file) {
    struct dedos_dfg *dfg = parse_dfg_json_file(test_dfg_path);
    ck_assert_ptr_ne(dfg, NULL);
    free_dfg(dfg);
} END_DEDOS_TEST


DEDOS_START_TEST_LIST("dfg_test")

DEDOS_ADD_TEST_RESOURCE(test_dfg_path, "test_dfg.json")
DEDOS_ADD_TEST_FN(test_fix_num_threads_incorrect)
DEDOS_ADD_TEST_FN(test_fix_num_threads_correct)
DEDOS_ADD_TEST_FN(test_parse_dfg_json_file)

DEDOS_END_TEST_LIST()
