#include <check.h>
#define LOG_DEDOS_STATISTICS 1

#include "dedos_statistics.c"
#include "logging.h"

#define LOG_STAT_SER_TEST 1

#define NUM_STAT_SAMPLES 16
#define NUM_STATS 100

START_TEST(test_stat_serialization) {

    struct stats_control_payload payload;

    payload.n_samples = NUM_STAT_SAMPLES;
    payload.samples = malloc(sizeof(struct stat_sample) * NUM_STAT_SAMPLES);

    for (int i=0; i<NUM_STAT_SAMPLES; i++){
        struct stat_sample *sample = &payload.samples[i];
        sample->stat_id = i % 3;
        sample->item_id = i / 3;

        sample->cur_time.tv_sec = i;
        sample->cur_time.tv_nsec = i;

        sample->n_stats = NUM_STATS;
        sample->stats = malloc(sizeof(struct timed_stat) * NUM_STATS);

        for (int j=0; j<NUM_STATS; j++){
            sample->stats[j].stat = j;
            sample->stats[j].time.tv_sec = i-j % 10;
            sample->stats[j].time.tv_nsec = i + j;
        }
    }

    char buff[MAX_STAT_BUFF_SIZE];

    log_custom(LOG_STAT_SER_TEST, "SERIALIZING");

    int rtn = serialize_stat_payload(&payload, buff);
    ck_assert(rtn >= 0);

    struct stats_control_payload payload_out;
    log_custom(LOG_STAT_SER_TEST, "DESERIALIZING");
    rtn = deserialize_stat_payload(buff, &payload_out);
    ck_assert(rtn >= 0);


    ck_assert_int_eq(payload_out.n_samples, payload.n_samples);
    for (int i=0; i<payload_out.n_samples; i++ ){
        struct stat_sample *sample_in = &payload.samples[i];
        struct stat_sample *sample_out = &payload_out.samples[i];

        ck_assert_int_eq(sample_in->stat_id, sample_out->stat_id);
        ck_assert_int_eq(sample_in->item_id, sample_out->item_id);

        ck_assert_int_eq(sample_in->cur_time.tv_sec, sample_out->cur_time.tv_sec);
        ck_assert_int_eq(sample_in->cur_time.tv_nsec, sample_out->cur_time.tv_nsec);

        ck_assert_int_eq(sample_in->n_stats, sample_out->n_stats);
        for (int j=0; j<NUM_STATS; j++){
            ck_assert_int_eq(sample_in->stats[j].stat, sample_out->stats[j].stat);
        }
    }
    log_custom(LOG_STAT_SER_TEST, "Deserialized matches serialized!");

} END_TEST

int main(int argc, char **argv) {
    Suite *s  = suite_create("stat_serialization_test");

    TCase *tc_stat = tcase_create("stat_ser");
    tcase_add_test(tc_stat, test_stat_serialization);
    suite_add_tcase(s, tc_stat);

    SRunner *sr;

    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : -1;
}
