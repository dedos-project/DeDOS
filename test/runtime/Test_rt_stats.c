#include <stdio.h>
#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "rt_stats.c"

#include <time.h>

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

struct stat_item *init_test_stat_item(int stat_id, int item_id) {
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

    struct stat_item *item = init_test_stat_item(sample_stat_id, sample_item_id);

    int rtn = record_start_time(sample_stat_id, sample_item_id);
    ck_assert(rtn == 0);

    ck_assert_msg(item->stats[0].time.tv_sec != 0 || item->stats[0].time.tv_nsec != 0,
                  "Recording start time did not modify recorded time values");
    ck_assert_msg(item->write_index == 0, "Recording start time advanced write index");
} END_DEDOS_TEST

START_DEDOS_TEST(test_record_end_time) {
    struct stat_item *item = init_test_stat_item(sample_stat_id, sample_item_id);

    int rtn = record_start_time(sample_stat_id, sample_item_id);
    ck_assert(rtn == 0);

    rtn = record_end_time(sample_stat_id, sample_item_id);
    ck_assert(rtn == 0);

    ck_assert_msg(item->write_index == 1, "Recording end time did not advance write index");
    ck_assert_msg(item->stats[0].value != 0, "Recording end time did not change stat value");

} END_DEDOS_TEST

START_DEDOS_TEST(test_increment_stat__normal) {
    struct stat_item *item = init_test_stat_item(sample_stat_id, sample_item_id);

    increment_stat(sample_stat_id, sample_item_id, 10);
    ck_assert_int_eq(item->write_index, 1);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 10);

    increment_stat(sample_stat_id, sample_item_id, -2);
    ck_assert_int_eq(item->write_index, 2);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 8);

} END_DEDOS_TEST

START_DEDOS_TEST(test_increment_stat__rollover) {
    struct stat_item *item = init_test_stat_item(sample_stat_id, sample_item_id);

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
    struct stat_item *item = init_test_stat_item(sample_stat_id, sample_item_id);

    record_stat(sample_stat_id, sample_item_id, 10, 1);
    ck_assert_int_eq(item->write_index, 1);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 10);

    record_stat(sample_stat_id, sample_item_id, 15, 1);
    ck_assert_int_eq(item->write_index, 2);
    ck_assert_int_eq(item->stats[item->write_index-1].value, 15);
} END_DEDOS_TEST

START_DEDOS_TEST(test_record_stat__rollover) {
    struct stat_item *item = init_test_stat_item(sample_stat_id, sample_item_id);

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
    struct stat_item *item = init_test_stat_item(sample_stat_id, sample_item_id);

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

void init_filled_stat_items(int stat_id, unsigned int *item_ids, int n_item_ids, int max_time) {
    int rtn = init_statistics();
    ck_assert(rtn == 0);

    struct stat_type *type = &stat_types[stat_id];
    ASSERT_ENABLED(type);

    for (int i=0; i<n_item_ids; i++) {

        rtn = init_stat_item(stat_id, item_ids[i]);
        ck_assert(rtn == 0);

        struct stat_item *item = get_item_stat(type, item_ids[i]);
        ck_assert(item != NULL);

        for (int j=0; j<max_time; j++) {
            item->stats[j].time.tv_sec = j;
            item->stats[j].value = j;
        }
        item->write_index = max_time;
    }
}

START_DEDOS_TEST(test_sample_stat_item) {

    int max_time = 500;
    init_filled_stat_items(sample_stat_id, &sample_item_id, 1, max_time);
    struct stat_item *item = get_item_stat(&stat_types[sample_stat_id], sample_item_id);

    struct timespec *end = &item->stats[max_time-1].time;
    int interval_s = 10;
    long interval_ns = 4e8;
    struct timespec interval = {.tv_sec = interval_s, .tv_nsec = interval_ns};
    int sample_size = 20;

    struct timed_stat *stats = malloc(sample_size * sizeof(*stats));
    int rtn = sample_stat_item(item, max_time, end, &interval, sample_size, stats);

    ck_assert_int_gt(rtn, 0);

    for (int i=0; i<sample_size; i++) {
        ck_assert_int_eq(stats[sample_size - i - 1].time.tv_sec,
                         max_time - (i) * interval_s - ((i *interval_ns * 1e-9))- 1);
    }

    free(stats);
} END_DEDOS_TEST

START_DEDOS_TEST(test_sample_stat) {
    unsigned int items[] = {0,1,2,3};
    int n_items = sizeof(items) / sizeof(*items);
    int max_time = 500;
    init_filled_stat_items(sample_stat_id, items, n_items, max_time);
    struct stat_item *item = get_item_stat(&stat_types[sample_stat_id], 0);
    struct timespec *end = &item->stats[max_time - 1].time;
    int interval_s = 1;
    struct timespec interval = {.tv_sec = interval_s};

    int max_sample_size = 30;
    struct stat_sample *samples = init_stat_samples(max_sample_size, n_items);
    ck_assert_ptr_ne(samples, NULL);

    int sample_size = 10;

    int rtn = sample_stat(sample_stat_id, end, &interval, sample_size, samples, n_items);
    ck_assert_int_eq(rtn, n_items);

    for (int i=0; i<n_items; i++) {
        ck_assert_int_eq(samples[i].hdr.stat_id, sample_stat_id);
        ck_assert_int_eq(samples[i].hdr.item_id, items[i]);
        ck_assert_int_eq(samples[i].hdr.n_stats, sample_size);
    }

    free_stat_samples(samples, n_items);

} END_DEDOS_TEST

char stat_output_path[64];

#define N_ITEMS 4
#define MAX_STATS_ 16384
START_DEDOS_TEST(test_output_stat_item) {
    struct stat_type type = {
        .id = 10,
        .enabled = true,
        .max_stats = MAX_STATS_,
        .format = "%09.9f",
        .label = "LONG_TEST_TITLE",
        .id_indices = {},
        .num_items = N_ITEMS
    };
    type.items = malloc(N_ITEMS * sizeof(*type.items));
    for (int i=0; i<N_ITEMS; i++) {
        type.id_indices[i] = i;
        type.items[i].id = i;
        type.items[i].write_index = rand() % MAX_STATS_;
        type.items[i].rolled_over = false;
        type.items[i].stats = malloc(MAX_STATS_ * sizeof(*type.items[i].stats));
        for (int j=0; j<1000; j++) {
            int idx = rand() % type.items[i].write_index;
            type.items[i].stats[idx].time.tv_sec = rand() % 1234;
            type.items[i].stats[idx].time.tv_nsec = rand() % 5678;
            type.items[i].stats[idx].value = (double) (rand() % 1000);
        }
    }

    FILE *file = fopen(stat_output_path, "w");
    for (int i=0; i<N_ITEMS; i++) {
        log(LOG_TEST, "HERE %d %d", i, type.items[i].write_index);
        write_item_to_log(file, &type, &type.items[i]);
    }
    fclose(file);

    for (int i=0; i<N_ITEMS; i++) {
        free(type.items[i].stats);
    }
    free(type.items);

} END_DEDOS_TEST


DEDOS_START_TEST_LIST("Statistics")
DEDOS_ADD_TEST_RESOURCE(stat_output_path, "stat_output.txt")

DEDOS_ADD_TEST_FN(test_output_stat_item);
DEDOS_ADD_TEST_FN(test_init_stat_item);;
DEDOS_ADD_TEST_FN(test_record_uninitialized_stat);
DEDOS_ADD_TEST_FN(test_record_start_time);
DEDOS_ADD_TEST_FN(test_record_end_time)
DEDOS_ADD_TEST_FN(test_increment_stat__normal)
DEDOS_ADD_TEST_FN(test_increment_stat__rollover)
DEDOS_ADD_TEST_FN(test_record_stat__normal)
DEDOS_ADD_TEST_FN(test_record_stat__rollover)
DEDOS_ADD_TEST_FN(test_record_stat__relogging)
DEDOS_ADD_TEST_FN(test_sample_stat_item)
DEDOS_ADD_TEST_FN(test_sample_stat)


DEDOS_END_TEST_LIST()
