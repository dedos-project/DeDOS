#include <stdio.h>
#include <check.h>
#include <openssl/ssl.h>
#include "logging.h"
#define STATLOG 1
#define LOG_STAT_INTERNALS 1
#define LOG_TEST_STAT 1

// Wheeee!
// (Access to static methods)
#include "dfg_reader.c"

#define TEST_JSON \
"{ \"MSUs\": [ { \"id\": \"1\", "\
"       \"dependencies\": [ { "\
"           \"id\": \"1\", "\
"           \"locality\": \"-1\" "\
"       }, { \"id\": \"2\", \"locality\": \"0\" } ] } , "\
"  { \"id\": \"2\", \"dependencies\": [ { \"id\": \"3\", \"locality\": \"1\" } ] } ] }    "



#define TEST_JSON_FILE "test_json.json"

int write_to_file(char *contents, char *filename){
    FILE *file = fopen(filename, "w");
    fwrite(contents, 1, strlen(contents), file);
    fclose(file);
    return 0;
}

START_TEST(test_dependencies) {

    write_to_file(TEST_JSON, TEST_JSON_FILE);

    struct dfg_config *cfg = malloc(sizeof(*cfg));
    bzero(cfg, sizeof(*cfg));

    int rtn = parse_into_obj(TEST_JSON_FILE, cfg, key_map);

    ck_assert(rtn==0);

    struct dfg_vertex *v1 = cfg->vertices[0];
    struct dfg_vertex *v2 = cfg->vertices[1];

    log_debug("V1 ID %d", v1->msu_id);
    log_debug("V2 ID %d", v2->msu_id);

    ck_assert_int_eq(v1->num_dependencies, 2);
    ck_assert(v1->dependencies[0]->msu_type == 1);
    ck_assert(v1->dependencies[0]->locality == -1);
    ck_assert(v1->dependencies[1]->msu_type == 2);
    ck_assert(v1->dependencies[1]->locality == 0);

    ck_assert(v2->dependencies[0]->msu_type == 3);
    ck_assert(v2->dependencies[0]->locality == 1);
} END_TEST



int main(int argc, char **argv) {
    Suite *s  = suite_create("dfg_test");

    TCase *tc_dep = tcase_create("dep");
    tcase_add_test(tc_dep, test_dependencies);
    suite_add_tcase(s, tc_dep);

    SRunner *sr;

    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : -1;
}

