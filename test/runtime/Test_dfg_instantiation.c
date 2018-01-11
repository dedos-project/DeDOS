#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "dfg_instantiation.c"

// See Test_dfg_instantiation.mk (and Makefile) for how I can do this
#include "routing.c"

#include <unistd.h>

struct dfg_runtime local_runtime = {
    .id = 12
};

struct dfg_runtime remote_runtime = {
    .id = 15
};

#define REMOTE_MSU(id_) \
{ \
    .id = id_, \
    .scheduling = { \
        .runtime = &remote_runtime \
    } \
}

struct dfg_msu msu_1 = REMOTE_MSU(1);
struct dfg_msu msu_2 = REMOTE_MSU(2);
struct dfg_msu msu_3 = REMOTE_MSU(3);

struct dfg_route_endpoint dest1 = {10, &msu_1};
struct dfg_route_endpoint dest2 = {20, &msu_2};
struct dfg_route_endpoint dest3 = {30, &msu_3};

struct dfg_route_endpoint *dests[] = {
    &dest1, &dest2, &dest3
};

struct dfg_route_endpoint dest4 = {5, &msu_3};
struct dfg_route_endpoint dest5 = {15, &msu_2};

struct dfg_route_endpoint *dests_2[] = {
    &dest4, &dest5
};

struct dfg_msu_type type = {
    .id = 1
};

struct dfg_route route1 = {
    .id = 11,
    .runtime = &local_runtime,
    .msu_type = &type,
    .endpoints = {&dest1, &dest2, &dest3},
    .n_endpoints = 3
};
struct dfg_route route2 = {
    .id = 22,
    .runtime = &local_runtime,
    .msu_type = &type,
    .endpoints = {&dest4, &dest5},
    .n_endpoints = 2
};

struct dfg_route *routes[] = {
    &route1, &route2
};

START_DEDOS_TEST(test_add_dfg_route_endpoints_success) {
    set_local_runtime(&local_runtime);
    int route_id = 6;

    ck_assert_int_eq(init_route(route_id, 500), 0);

    struct routing_table *table = get_routing_table(route_id);
    ck_assert(table != NULL);

    ck_assert_int_eq(add_dfg_route_endpoints(route_id, dests, 3), 0);

    ck_assert_int_eq(table->n_endpoints, 3);
    ck_assert_int_eq(table->keys[0], dest1.key);
    ck_assert_int_eq(table->keys[1], dest2.key);
    ck_assert_int_eq(table->keys[2], dest3.key);

    ck_assert_int_eq(table->endpoints[0].id, dest1.msu->id);
    ck_assert_int_eq(table->endpoints[1].id, dest2.msu->id);
    ck_assert_int_eq(table->endpoints[2].id, dest3.msu->id);

} END_DEDOS_TEST

START_DEDOS_TEST(test_spawn_dfg_routes) {
    set_local_runtime(&local_runtime);

    ck_assert_int_eq(spawn_dfg_routes(routes, 2), 0);

    struct routing_table *table1 = get_routing_table(route1.id);
    ck_assert(table1 != NULL);
    struct routing_table *table2 = get_routing_table(route2.id);
    ck_assert(table2 != NULL);
    struct routing_table *table_NA = get_routing_table(33);
    ck_assert(table_NA == NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_add_dfg_routes_to_msu) {
    set_local_runtime(&local_runtime);

    spawn_dfg_routes(routes, 2);

    struct local_msu msu = {};

    ck_assert_int_eq(add_dfg_routes_to_msu(&msu, routes, 2), 0);

    ck_assert_int_eq(msu.routes.n_routes, 2);

    free(msu.routes.routes);
} END_DEDOS_TEST

// TODO: test_spawn_dfg_msus
// TODO: test_spawn_dfg_threads

DEDOS_START_TEST_LIST("dfg_instantiation")

DEDOS_ADD_TEST_FN(test_add_dfg_route_endpoints_success)
DEDOS_ADD_TEST_FN(test_spawn_dfg_routes)
DEDOS_ADD_TEST_FN(test_add_dfg_routes_to_msu)

DEDOS_END_TEST_LIST()
