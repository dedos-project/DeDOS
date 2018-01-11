#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "routing.c"

struct msu_endpoint ep0 = {.id = 1};
struct msu_endpoint ep1 = {.id = 2};
struct msu_endpoint ep2 = {.id = 3};

struct routing_table *test_table() {
    struct routing_table *table = init_routing_table(1, 1);

    table->keys[0] = 5;
    table->keys[1] = 10;
    table->keys[2] = 20;

    table->endpoints[0] = ep0;
    table->endpoints[1] = ep1;
    table->endpoints[2] = ep2;

    table->n_endpoints = 3;

    return table;
}

START_DEDOS_TEST(test_find_value_index) {
    struct routing_table *table = test_table();

    ck_assert_int_eq(find_value_index(table, 0), 0);
    ck_assert_int_eq(find_value_index(table, 4), 0);
    ck_assert_int_eq(find_value_index(table, 5), 1);
    ck_assert_int_eq(find_value_index(table, 9), 1);
    ck_assert_int_eq(find_value_index(table, 10), 2);
    ck_assert_int_eq(find_value_index(table, 19), 2);
    ck_assert_int_eq(find_value_index(table, 20), 0);
    ck_assert_int_eq(find_value_index(table, 25), 1);
    ck_assert_int_eq(find_value_index(table, 30), 2);

    free(table);
} END_DEDOS_TEST

START_DEDOS_TEST(test_find_id_index) {
    struct routing_table *table = test_table();

    ck_assert_int_eq(find_id_index(table, 1), 0);
    ck_assert_int_eq(find_id_index(table, 2), 1);
    ck_assert_int_eq(find_id_index(table, 3), 2);
    ck_assert_int_eq(find_id_index(table, 4), -1);

    free(table);
} END_DEDOS_TEST

START_DEDOS_TEST(test_rm_routing_table_entry) {
    struct routing_table *table = test_table();

    ck_assert_int_eq(rm_routing_table_entry(table, 2), 0);

    ck_assert_int_eq(table->keys[0], 5);
    ck_assert_int_eq(table->endpoints[0].id, 1);
    ck_assert_int_eq(table->keys[1], 20);
    ck_assert_int_eq(table->endpoints[1].id, 3);
    ck_assert_int_eq(table->n_endpoints, 2);

    free(table);
} END_DEDOS_TEST

START_DEDOS_TEST(test_add_routing_table_entry) {
    struct routing_table *table = test_table();

    struct msu_endpoint ep3 = {.id = 4};
    ck_assert_int_eq(add_routing_table_entry(table, &ep3, 15), 0);

    ck_assert_int_eq(table->keys[1], 10);
    ck_assert_int_eq(table->endpoints[1].id, 2);
    ck_assert_int_eq(table->keys[2], 15);
    ck_assert_int_eq(table->endpoints[2].id, 4);
    ck_assert_int_eq(table->keys[3], 20);
    ck_assert_int_eq(table->endpoints[3].id, 3);
    ck_assert_int_eq(table->n_endpoints, 4);

    free(table);
} END_DEDOS_TEST

START_DEDOS_TEST(test_alter_routing_table_entry) {
    struct routing_table *table = test_table();

    ck_assert_int_eq(alter_routing_table_entry(table, 1, 15), 0);

    ck_assert_int_eq(table->keys[0], 10);
    ck_assert_int_eq(table->endpoints[0].id, 2);
    ck_assert_int_eq(table->keys[1], 15);
    ck_assert_int_eq(table->endpoints[1].id, 1);
    ck_assert_int_eq(table->keys[2], 20);
    ck_assert_int_eq(table->endpoints[2].id, 3);
    ck_assert_int_eq(table->n_endpoints, 3);

    free(table);
} END_DEDOS_TEST

START_DEDOS_TEST(test_init_route_success) {
    ck_assert_int_eq(init_route(10, 100), 0);

    struct routing_table *table = get_routing_table(10);
    ck_assert(table != NULL);
    ck_assert_int_eq(table->id, 10);
    ck_assert_int_eq(table->type_id, 100);
} END_DEDOS_TEST

START_DEDOS_TEST(test_init_route_fail_duplicate) {
    ck_assert_int_eq(init_route(10, 100), 0);
    ck_assert_int_eq(init_route(10, 100), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_endpoint_by_index_success) {
    struct routing_table *table = test_table();
    struct msu_endpoint ep;
    ck_assert_int_eq(get_endpoint_by_index(table, 0, &ep), 0);
    ck_assert(ep.id == ep0.id);
    ck_assert_int_eq(get_endpoint_by_index(table, 1, &ep), 0);
    ck_assert(ep.id == ep1.id);

    free(table);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_endpoint_by_index_fail) {
    struct routing_table *table = test_table();
    struct msu_endpoint ep;
    ck_assert_int_eq(get_endpoint_by_index(table, 4, &ep), -1);

    free(table);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_endpoint_by_id_success) {
    struct routing_table *table = test_table();
    struct msu_endpoint ep;
    ck_assert_int_eq(get_endpoint_by_id(table, 1, &ep), 0);
    ck_assert(ep.id = ep0.id);
    ck_assert_int_eq(get_endpoint_by_id(table, 3, &ep), 0);
    ck_assert(ep.id = ep2.id);
    free(table);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_endpoint_by_id_fail) {
    struct routing_table *table = test_table();
    struct msu_endpoint ep;
    ck_assert_int_eq(get_endpoint_by_id(table, 4, &ep), -1);
    ck_assert_int_eq(get_endpoint_by_id(table, 0, &ep), -1);

    free(table);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_route_endpoint_success) {
    struct routing_table *table = test_table();
    struct msu_endpoint ep;
    ck_assert_int_eq(get_route_endpoint(table, 15, &ep), 0);
    ck_assert_int_eq(ep.id, ep2.id);

    free(table);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_type_from_route_set_success) {
    struct routing_table *table = test_table();
    struct routing_table *table2 = init_routing_table(2, 9);

    struct route_set set = {.n_routes = 2};
    set.routes = calloc(2, sizeof(table));
    set.routes[0] = table;
    set.routes[1] = table2;

    struct routing_table *out = get_type_from_route_set(&set, 1);
    ck_assert(out == table);
    out = get_type_from_route_set(&set, 9);
    ck_assert(out == table2);

    free(set.routes);
    free(table);
    free(table2);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_type_from_route_set_fail) {
    struct routing_table *table = test_table();
    struct routing_table *table2 = init_routing_table(2, 9);

    struct route_set set = {.n_routes = 2};
    set.routes = calloc(2, sizeof(table));
    set.routes[0] = table;
    set.routes[1] = table2;

    struct routing_table *out = get_type_from_route_set(&set, 2);
    ck_assert(out == NULL);
    free(table);
    free(table2);
    free(set.routes);
} END_DEDOS_TEST

START_DEDOS_TEST(test_add_route_to_set_success) {
    init_route(10, 100);
    init_route(11, 110);
    struct route_set set = {.n_routes = 0};
    ck_assert_int_eq(add_route_to_set(&set, 10), 0);
    ck_assert_int_eq(set.n_routes, 1);
    ck_assert_int_eq(set.routes[0]->id, 10);
    ck_assert_int_eq(set.routes[0]->type_id, 100);

    ck_assert_int_eq(add_route_to_set(&set, 11), 0);
    ck_assert_int_eq(set.n_routes, 2);
    ck_assert_int_eq(set.routes[1]->id, 11);
    ck_assert_int_eq(set.routes[1]->type_id, 110);

    free(set.routes);
} END_DEDOS_TEST

START_DEDOS_TEST(test_add_route_to_set_fail) {
    init_route(10, 100);
    struct route_set set = {.n_routes = 0};
    ck_assert_int_eq(add_route_to_set(&set, 11), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_rm_route_from_set_success) {
    struct routing_table *table = test_table();
    struct routing_table *table2 = init_routing_table(10, 9);

    struct route_set set = {.n_routes = 2};
    set.routes = calloc(2, sizeof(table));
    set.routes[0] = table;
    set.routes[1] = table2;

    ck_assert_int_eq(rm_route_from_set(&set, 10), 0);
    ck_assert_int_eq(set.n_routes, 1);
    ck_assert_int_eq(set.routes[0]->id, 1);

    ck_assert_int_eq(rm_route_from_set(&set, 1), 0);
    ck_assert_int_eq(set.n_routes, 0);
    free(set.routes);
    free(table);
    free(table2);
} END_DEDOS_TEST

START_DEDOS_TEST(test_rm_route_from_set_fail) {
    struct routing_table *table = test_table();
    struct routing_table *table2 = init_routing_table(10, 9);

    struct route_set set = {.n_routes = 2};
    set.routes = calloc(2, sizeof(table));
    set.routes[0] = table;
    set.routes[1] = table2;

    ck_assert_int_eq(rm_route_from_set(&set, 11), -1);
    ck_assert_int_eq(rm_route_from_set(&set, 10), 0);
    ck_assert_int_eq(rm_route_from_set(&set, 10), -1);

    free(set.routes);
    free(table);
    free(table2);
} END_DEDOS_TEST

DEDOS_START_TEST_LIST("routing")

DEDOS_ADD_TEST_FN(test_find_value_index);
DEDOS_ADD_TEST_FN(test_find_id_index);
DEDOS_ADD_TEST_FN(test_rm_routing_table_entry);
DEDOS_ADD_TEST_FN(test_add_routing_table_entry);
DEDOS_ADD_TEST_FN(test_alter_routing_table_entry);
DEDOS_ADD_TEST_FN(test_init_route_success);
DEDOS_ADD_TEST_FN(test_init_route_fail_duplicate);
DEDOS_ADD_TEST_FN(test_get_endpoint_by_index_success);
DEDOS_ADD_TEST_FN(test_get_endpoint_by_index_fail);
DEDOS_ADD_TEST_FN(test_get_endpoint_by_id_success);
DEDOS_ADD_TEST_FN(test_get_endpoint_by_id_fail);
DEDOS_ADD_TEST_FN(test_get_route_endpoint_success);
DEDOS_ADD_TEST_FN(test_get_type_from_route_set_fail);
DEDOS_ADD_TEST_FN(test_get_type_from_route_set_success);
DEDOS_ADD_TEST_FN(test_add_route_to_set_success);
DEDOS_ADD_TEST_FN(test_add_route_to_set_fail);
DEDOS_ADD_TEST_FN(test_rm_route_from_set_success);
DEDOS_ADD_TEST_FN(test_rm_route_from_set_fail);
    
DEDOS_END_TEST_LIST()
