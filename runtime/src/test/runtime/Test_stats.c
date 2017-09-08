#include <stdio.h>
#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "stats.c"

#define sample_stat_id stat_types[0].id
unsigned int sample_item_id = 5;

#define ASSERT_ENABLED(type) \
    ck_assert_msg(type->enabled, "Type not enabled, test invalid")

START_DEDOS_TEST(test_init_stat_item) {
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
} END_DEDOS_TEST

START_DEDOS_TEST(test_record_uninitialized_stat) {
    int rtn = init_statistics();
    ck_assert(rtn ==0);

    struct stat_type *type = &stat_types[sample_stat_id];
    ASSERT_ENABLED(type);

    rtn = record_start_time(sample_stat_id, sample_item_id);
    ck_assert(rtn == -1);
} END_DEDOS_TEST

struct stat_item *test_init_item_stat(int stat_id, int item_id) {
    int rtn = init_statistics();
    ck_assert(rtn == 0);

    struct stat_type *type = &stat_types[stat_id];
    ASSERT_ENABLED(type);

    rtn = init_stat_item(stat_id, item_id);
    ck_assert(rtn == 0);

    struct stat_item *item = get_item_stat(type, item_id);
    ck_assert(item != NULL);
 
    return item;
}

START_DEDOS_TEST(test_record_start_time) {
    
    struct stat_item *item = test_init_item_stat(sample_stat_id, sample_item_id);

    int rtn = record_start_time(sample_stat_id, sample_item_id);
    ck_assert(rtn == 0);

    ck_assert_msg(item->stats[0].time.tv_sec != 0 || item->stats[0].time.tv_nsec != 0,
                  "Recording start time did not modify recorded time values");
    ck_assert_msg(item->write_index == 0, "Recording start time advanced write index");
} END_DEDOS_TEST

START_DEDOS_TEST(test_record_end_time) {
    struct stat_item *item = test_init_item_stat(sample_stat_id, sample_item_id);

    int rtn = record_start_time(sample_stat_id, sample_item_id);
    ck_assert(rtn == 0);

    rtn = record_end_time(sample_stat_id, sample_item_id);
    ck_assert(rtn == 0);

    ck_assert_msg(item->write_index == 1, "Recording end time did not advance write index");
    ck_assert_msg(item->stats[0].value != 0, "Recording end time did not change stat value");

} END_DEDOS_TEST

START_DEDOS_TEST(test_increment_stat__normal) {
    struct stat_item *item = test_init_item_stat(sample_stat_id, sample_item_id);
    
    increment_stat(sample_stat_id, sample_item_id, 10);
    ck_assert_int_eq(item->write_index, 1);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 10);

    increment_stat(sample_stat_id, sample_item_id, -2);
    ck_assert_int_eq(item->write_index, 2);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 8);

} END_DEDOS_TEST

START_DEDOS_TEST(test_increment_stat__rollover) {
    struct stat_item *item = test_init_item_stat(sample_stat_id, sample_item_id);

    int stat1 = 15;
    int stat2 = 32;
    int stat3 = 12;

    int max_stats = stat_types[sample_stat_id].max_stats;
    item->write_index = max_stats - 2;

    increment_stat(sample_stat_id, sample_item_id, stat1);
    ck_assert_int_eq(item->write_index, max_stats - 1);
    ck_assert_int_eq(item->stats[max_stats - 2].value, stat1);

    increment_stat(sample_stat_id, sample_item_id, stat2);
    ck_assert_int_eq(item->write_index, 0);
    ck_assert_int_eq(item->stats[max_stats - 1].value, stat2 + stat1);

    increment_stat(sample_stat_id, sample_item_id, stat3);
    ck_assert_int_eq(item->write_index, 1);
    ck_assert_int_eq(item->stats[0].value, stat3 + stat2 + stat1);

} END_DEDOS_TEST

START_DEDOS_TEST(test_record_stat__normal) {
    struct stat_item *item = test_init_item_stat(sample_stat_id, sample_item_id);

    record_stat(sample_stat_id, sample_item_id, 10, 1);
    ck_assert_int_eq(item->write_index, 1);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 10);

    record_stat(sample_stat_id, sample_item_id, 15, 1);
    ck_assert_int_eq(item->write_index, 2);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 15);
} END_DEDOS_TEST

START_DEDOS_TEST(test_record_stat__rollover) {
    struct stat_item *item = test_init_item_stat(sample_stat_id, sample_item_id);

    int stat1 = 15;
    int stat2 = 32;
    int stat3 = 12;

    int max_stats = stat_types[sample_stat_id].max_stats;
    item->write_index = max_stats - 2;

    record_stat(sample_stat_id, sample_item_id, stat1, 1);
    ck_assert_int_eq(item->write_index, max_stats - 1);
    ck_assert_int_eq(item->stats[max_stats - 2].value, stat1);

    record_stat(sample_stat_id, sample_item_id, stat2, 1);
    ck_assert_int_eq(item->write_index, 0);
    ck_assert_int_eq(item->stats[max_stats - 1].value, stat2);

    record_stat(sample_stat_id, sample_item_id, stat3, 1);
    ck_assert_int_eq(item->write_index, 1);
    ck_assert_int_eq(item->stats[0].value, stat3);

} END_DEDOS_TEST

START_DEDOS_TEST(test_record_stat__relogging) {
    struct stat_item *item = test_init_item_stat(sample_stat_id, sample_item_id);

    record_stat(sample_stat_id, sample_item_id, 10, 0);
    ck_assert_int_eq(item->write_index, 1);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 10);

    record_stat(sample_stat_id, sample_item_id, 10, 0);
    ck_assert_int_eq(item->write_index, 1);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 10);

    record_stat(sample_stat_id, sample_item_id, 10, 1);
    ck_assert_int_eq(item->write_index, 2);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 10);

    record_stat(sample_stat_id, sample_item_id, 11, 0);
    ck_assert_int_eq(item->write_index, 3);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 11);

} END_DEDOS_TEST


DEDOS_START_TEST_LIST("Statistics")

DEDOS_ADD_TEST_FN(test_init_stat_item);
DEDOS_ADD_TEST_FN(test_record_uninitialized_stat);
DEDOS_ADD_TEST_FN(test_record_start_time);
DEDOS_ADD_TEST_FN(test_record_end_time)
DEDOS_ADD_TEST_FN(test_increment_stat__normal)
DEDOS_ADD_TEST_FN(test_increment_stat__rollover)
DEDOS_ADD_TEST_FN(test_record_stat__normal)
DEDOS_ADD_TEST_FN(test_record_stat__rollover)
DEDOS_ADD_TEST_FN(test_record_stat__relogging)

DEDOS_END_TEST_LIST()
