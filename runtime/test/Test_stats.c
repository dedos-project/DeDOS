#include <stdio.h>
#include <check.h>
#include <openssl/ssl.h>
#include "logging.h"
#define STATLOG 1
#define LOG_STAT_INTERNALS 1
#define LOG_TEST_STAT 1

// Wheeee!
// (Need static methods...)
#include "stats.c"

// Unfortunately, need to declare this for compilation
SSL_CTX* ssl_ctx_global;

enum stat_id test_stat_id = QUEUE_LEN;
int test_item_id = 1;
int sample_size = 100;
int interval_ms = 10; // .01 seconds
int n_gathered_stats = 20000; // 20 seconds
int sample_duration_s = 25;
int end_time_ms;

static void fill_stats(){
    struct item_stats *item = &saved_stats[test_stat_id].item_stats[test_item_id];
    struct timed_stat *stats = item->stats;
    time_t curtime_s = 0;
    long curtime_ms = 0;
    for (int i=0; i<n_gathered_stats; i++){
        curtime_ms += interval_ms;
        if (curtime_ms >= 1000) {
            curtime_s += 1;
            curtime_ms %= 1000;
        }
        stats[i].stat = i;
        stats[i].time.tv_sec = curtime_s;
        stats[i].time.tv_nsec = curtime_ms * 1000000;
    }
    item->n_stats = n_gathered_stats;
    item->last_sample_index = n_gathered_stats-1;
    end_time_ms = curtime_s * 1000 + curtime_ms / 1000000;
}


START_TEST(test_sample_stats){
    init_statlog(NULL);

    fill_stats();

    struct timed_stat *sample = sample_stats(test_stat_id, test_item_id, sample_duration_s, sample_size);

    int time_start_ms = sample[0].time.tv_sec * 1000 + sample[0].time.tv_nsec / 1000000;
    int time_end_ms = sample[sample_size-1].time.tv_sec * 1000 + \
                      sample[sample_size-1].time.tv_nsec / 1000000;

    log_custom(LOG_TEST_STAT, "end_time_ms %d time_end_ms %d", end_time_ms, time_end_ms);
    ck_assert_msg( (end_time_ms -time_end_ms ) <= interval_ms,
                   "Unexpected interval");
    ck_assert_msg( (time_end_ms - time_start_ms ) - sample_duration_s * 1000 <= interval_ms,
                    "Unexpected duration");

    int second_ms = sample[1].time.tv_sec * 1000 + \
                      sample[1].time.tv_nsec / 1000000;

    int sample_interval = second_ms - time_start_ms;

    int last_ms = time_start_ms;
    for (int i=1; i<sample_size; i++){
        int this_ms = sample[i].time.tv_sec * 1000 + \
                      sample[i].time.tv_nsec / 1000000;

        int this_interval = this_ms - last_ms;

        ck_assert_msg(this_interval - sample_interval <= interval_ms &&
                     sample_interval - this_interval <= interval_ms,
                     "Unexpected internal duration");

        last_ms = this_ms;
    }
} END_TEST

int main(int argc, char **argv) {
    Suite *s  = suite_create("stats_test");

    TCase *tc_stats = tcase_create("stats_sample");
    tcase_add_test(tc_stats, test_sample_stats);
    suite_add_tcase(s, tc_stats);

    SRunner *sr;

    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : -1;
}

