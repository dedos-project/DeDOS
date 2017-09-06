#include <stdio.h>
#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "stats.c"

#define sample_stat_id stat_types[0].id
unsigned int sample_item_id = 5;

#define ASSERT_ENABLED(type) \
    ck_assert_msg(type->enabled, "Type not enabled, test invalid")

START_TEST(test_init_stat_item) {
    int rtn = init_statistics();
    ck_assert(rtn == 0);

    struct stat_type *type = &stat_types[sample_stat_id];
    ASSERT_ENABLED(type);

    ck_assert_msg(type->id_indices[sample_item_id] == -1, "Item started out initialized");
    ck_assert_msg(type->num_items == 0, "Stat type started with items initialized");

    rtn = init_stat_item(sample_stat_id, sample_item_id);
    ck_assert_msg(rtn == 0, "Stat item initialization failed");

    struct stat_item *item = get_item_stat(type, sample_item_id);
    ck_assert_msg(item != NULL, "Could not retrieve initialized item stat");
    ck_assert_msg(item->write_index == 0, "Item started with non-0 write index");
    ck_assert_msg(!item->rolled_over, "Item started rolled-over");
    ck_assert_msg(item->id == sample_item_id, "Item assigned wrong ID");
    ck_assert_msg(item->stats[0].time.tv_sec == 0 && item->stats[0].time.tv_nsec == 0,
                  "Stat times not initialized to 0");
} END_TEST

START_TEST(test_record_uninitialized_stat) {
    int rtn = init_statistics();
    ck_assert(rtn ==0);

    struct stat_type *type = &stat_types[sample_stat_id];
    ASSERT_ENABLED(type);

    rtn = record_start_time(sample_stat_id, sample_item_id);
    ck_assert(rtn == -1);
} END_TEST

START_TEST(test_record_start_time) {
    int rtn = init_statistics();
    ck_assert(rtn == 0);

    struct stat_type *type = &stat_types[sample_stat_id];
    ASSERT_ENABLED(type);

    rtn = init_stat_item(sample_stat_id, sample_item_id);
    ck_assert(rtn == 0);

    struct stat_item *item = get_item_stat(type, sample_item_id);
    ck_assert(item != NULL);

    rtn = record_start_time(sample_stat_id, sample_item_id);
    ck_assert(rtn == 0);

    ck_assert_msg(item->stats[0].time.tv_sec != 0 || item->stats[0].time.tv_nsec != 0,
                  "Recording start time did not modify recorded time values");
    ck_assert_msg(item->write_index == 0, "Recording start time advanced write index");
} END_TEST

START_TEST(test_record_end_time) {
    int rtn = init_statistics();
    ck_assert(rtn == 0);

    struct stat_type *type = &stat_types[sample_stat_id];
    ASSERT_ENABLED(type);

    rtn = init_stat_item(sample_stat_id, sample_item_id);
    ck_assert(rtn == 0);

    struct stat_item *item = get_item_stat(type, sample_item_id);
    ck_assert(item != NULL);

    rtn = record_start_time(sample_stat_id, sample_item_id);
    ck_assert(rtn == 0);

    rtn = record_end_time(sample_stat_id, sample_item_id);
    ck_assert(rtn == 0);

    ck_assert_msg(item->write_index == 1, "Recording end time did not advance write index");
    ck_assert_msg(item->stats[0].value != 0, "Recording end time did not change stat value");

} END_TEST




int main(int argc, char **argv) {
    Suite *s  = suite_create("stats_test");

    TCase *tc_stats = tcase_create("stats_sample");
    //tcase_add_test(tc_stats, test_sample_item_stats);
    tcase_add_test(tc_stats, test_init_stat_item);
    tcase_add_test(tc_stats, test_record_uninitialized_stat);
    tcase_add_test(tc_stats, test_record_start_time);
    tcase_add_test(tc_stats, test_record_end_time);
    suite_add_tcase(s, tc_stats);

    SRunner *sr;

    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : -1;
}

