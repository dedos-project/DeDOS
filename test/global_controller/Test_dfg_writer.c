#define REPORTED_MSU_STAT_TYPES \
    {MSU_QUEUE_LEN, "TEST_QUEUE_LEN"}, \
    {MSU_MEM_ALLOC, "TEST_MEM_ALLOC"}

#define REPORTED_THREAD_STAT_TYPES

#define JSMN_STRICT

#include "dedos_testing.h"

#include "controller_dfg.c"
#include "dfg_writer.c"
#include "controller_stats.c"
#include "jsmn.c"

struct dfg_msu_type type_a = {
    .id = 1
};

struct dfg_msu_type type_b = {
    .id = 2
};

struct dfg_msu_type type_c = {
    .id = 3
};

struct dfg_meta_routing meta_routing = {
    .src_types = { &type_a, &type_b},
    .n_src_types = 2,
    .dst_types = { &type_b, &type_c},
    .n_dst_types = 2
};

struct dfg_dependency dep1 = {
    .type = &type_a,
    .locality = MSU_IS_LOCAL
};

struct dfg_dependency dep2 = {
    .type = &type_b,
    .locality = MSU_IS_REMOTE
};

struct dfg_thread thread1 = {
    .id = 1
};

struct dfg_msu msu_lite = {
    .id = 6
};

struct dfg_msu msu_lite2 = {
    .id = 7
};

struct dfg_route_endpoint ep1 = {
    .key = 1,
    .msu = &msu_lite
};

struct dfg_route_endpoint ep2 = {
    .key = 66,
    .msu = &msu_lite2
};

struct dfg_route route1 = {
    .id = 1,
    .msu_type = &type_a,
    .endpoints = {&ep1},
    .n_endpoints = 1
};

struct dfg_route route2 = {
    .id = 2,
    .msu_type = &type_b,
    .endpoints = {&ep1,
                  &ep2},
    .n_endpoints = 2
};

struct dfg_runtime rt1 = {
    .id = 1,
    .ip = 0,
    .port = 1234,
    .n_cores = 10,
    .n_pinned_threads = 1,
    .n_unpinned_threads = 0,
    .threads = {&thread1},
    .routes = {&route1, &route2},
    .n_routes = 2
};

struct dfg_runtime rt2 = {
    .id = 2,
    .ip = 1,
    .port = 1234,
    .routes = {&route2, &route1},
    .n_routes = 2
};

struct dfg_msu_type msu_type = {
    .id = 12,
    .name = "test_type",
    .meta_routing = {},
    .dependencies = {&dep1, &dep2},
    .n_dependencies = 2,
    .cloneable = 1,
    .colocation_group = 3
};

struct dfg_msu msu1 = {
    .id = 23,
    .vertex_type = ENTRY_VERTEX_TYPE,
    .init_data = {"MY_INIT_DATA"},
    .type = &msu_type,
    .blocking_mode = BLOCKING_MSU,
    .scheduling = {
        .runtime = &rt1,
        .thread =  &thread1,
        .routes = {&route1, &route2},
        .n_routes = 2
    }
};

struct dfg_msu msu2 = {
    .id = 35,
    .vertex_type = ENTRY_VERTEX_TYPE,
    .init_data = {"MY_INIT_DATA2"},
    .type = &msu_type,
    .blocking_mode = NONBLOCK_MSU,
    .scheduling = {
        .runtime = &rt1,
        .thread =  &thread1,
        .routes = {&route1},
        .n_routes = 1
    }
};

struct dedos_dfg dfg = {
    .application_name = "test_app",
    .global_ctl_ip = 0,
    .global_ctl_port = 2345,
    .msu_types = {&msu_type},
    .n_msu_types = 1,
    .msus = {&msu1, &msu2},
    .n_msus = 2,
    .runtimes = {&rt1, &rt2},
    .n_runtimes = 2
};


struct timed_rrdb stats = {
    .data = {1.23, 4.56, 7.89, 10.1112},
    .time = {{1}, {2}, {3}, {4}},
    .write_index = 5
};

void validate_json(char *json) {
    jsmn_parser parser;
    jsmn_init(&parser);
    int rtn = jsmn_parse(&parser, json, strlen(json), NULL, 0);
    log(TEST, "%s", json);
    log(TEST, "rtn: %d", rtn);
    jsmntok_t toks[rtn];
    jsmn_parser parser2;
    jsmn_init(&parser2);
    rtn = jsmn_parse(&parser2, json, strlen(json), toks, rtn);
    ck_assert_int_gt(rtn, 0);
}

START_DEDOS_TEST(test_meta_routing_to_json) {
    validate_json(meta_routing_to_json(&meta_routing));
} END_DEDOS_TEST

START_DEDOS_TEST(test_msu_type_to_json) {
    validate_json(msu_type_to_json(&msu_type));
} END_DEDOS_TEST

START_DEDOS_TEST(test_msu_to_json) {
    validate_json(msu_to_json(&msu1));
} END_DEDOS_TEST

START_DEDOS_TEST(test_runtime_to_json) {
    validate_json(runtime_to_json(&rt1));
} END_DEDOS_TEST

START_DEDOS_TEST(test_dfg_to_json__no_stats) {
    validate_json(dfg_to_json(&dfg));
} END_DEDOS_TEST


DEDOS_START_TEST_LIST("dfg_writer");
// From controller_dfg.c
DFG = &dfg;
DEDOS_ADD_TEST_FN(test_meta_routing_to_json)
DEDOS_ADD_TEST_FN(test_msu_type_to_json)
DEDOS_ADD_TEST_FN(test_msu_to_json)
DEDOS_ADD_TEST_FN(test_runtime_to_json)
DEDOS_ADD_TEST_FN(test_dfg_to_json__no_stats)
DEDOS_END_TEST_LIST()
