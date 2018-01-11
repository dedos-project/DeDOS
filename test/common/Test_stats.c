#include "dedos_testing.h"

#include "stats.c"

#include <stdlib.h>


void init_random_sample(struct stat_sample *sample, int item_id) {
    int num_stats = sample->max_stats / 2;
    sample->hdr.n_stats = num_stats;
    sample->hdr.stat_id = MSU_QUEUE_LEN;
    sample->hdr.item_id = item_id;
    for (int i=0; i<num_stats; i++) {
        sample->stats[i].time.tv_sec = i;
        sample->stats[i].value = rand();
    }
}

void assert_samples_match(struct stat_sample *a, struct stat_sample *b) {
    ck_assert_int_eq(a->hdr.stat_id, b->hdr.stat_id);
    ck_assert_int_eq(a->hdr.item_id, b->hdr.item_id);
    ck_assert_int_eq(a->hdr.n_stats, b->hdr.n_stats);

    for (int i=0; i<a->hdr.n_stats; i++) {
        ck_assert_int_eq(a->stats[i].time.tv_sec, b->stats[i].time.tv_sec);
        ck_assert_int_eq(a->stats[i].time.tv_nsec, b->stats[i].time.tv_nsec);
        ck_assert_int_eq(a->stats[i].value, b->stats[i].value);
    }
}

int n_samples = 48;
int max_stats = 24;
START_DEDOS_TEST(test_serialize_and_deserialize_stat_sample) {
    struct stat_sample *samples = calloc(n_samples,  sizeof(*samples));
    for (int i=0; i<n_samples; i++) {
        init_stat_sample(max_stats, &samples[i]);
        init_random_sample(&samples[i], i);
    }

    size_t buffer_size = serialized_stat_sample_size(samples, n_samples);
    log(TEST, "Buffer size is %d", (int)buffer_size);
    void *buffer = malloc(buffer_size);

    ssize_t used_size = serialize_stat_samples(samples, n_samples, buffer, buffer_size);

    ck_assert_int_eq(used_size, buffer_size);

    struct stat_sample *samples_out = calloc(n_samples, sizeof(*samples));
    for (int i=0; i<n_samples; i++) {
        init_stat_sample(max_stats, &samples_out[i]);
    }
    int n_samples_out  = deserialize_stat_samples(buffer, buffer_size, samples_out, n_samples);
    ck_assert_int_eq(n_samples_out, n_samples);
    for (int i=0; i<n_samples; i++) {
        assert_samples_match(&samples_out[i], &samples[i]);
    }
    free(samples);
    free(samples_out);
    free(buffer);
    for (int i=0; i < n_samples; i++) {
        free(samples[i].stats);
        free(samples_out[i].stats);
    }
} END_DEDOS_TEST

DEDOS_START_TEST_LIST("stats_test");

DEDOS_ADD_TEST_FN(test_serialize_and_deserialize_stat_sample);

DEDOS_END_TEST_LIST()

