#include "routing.h"
#include "global_controller/dfg.h"
#include "dfg_interpreter.h"
#include "testing_utils.h"
#include "runtime.h"
#include "msu_tracker.h"
#include <stdio.h>
#include <check.h>

// Forward declarations of functions under test:
struct runtime_endpoint *get_local_runtime(struct dfg_config *dfg, uint32_t local_ip);
int create_vertex_routes(struct dfg_vertex *vertex);
int create_route_from_vertices(struct dfg_vertex *from, struct dfg_vertex *to);
struct dedos_thread_msg *route_msg_from_vertices(struct dfg_vertex *from,
                                                 struct dfg_vertex *to);
struct runtime_endpoint *get_local_runtime(struct dfg_config *dfg, uint32_t local_ip);
int create_msu_from_vertex(struct dfg_vertex *vertex);
struct dedos_thread_msg *msu_msg_from_vertex(struct dfg_vertex *vertex);



// Pretending local address is 158.130.57.63
#define MK_IP(a, b, c, d) \
    ((uint32_t)(a<<24)) + ((uint32_t)(b<<16)) + ((uint32_t)(c<<8)) + ((uint32_t)(d))

#define LOCAL_IP_STR "158.130.57.63"
#define DFG_CONFIG_FILE "sample_dfg.json"

struct dfg_config *load_dfg(char *dfg_path){
    if (dfg_path == NULL)
        ck_abort_msg("No DFG file to load");
    mark_point();
    int rtn = do_dfg_config(dfg_path);
    mark_point();
    if (rtn < 0){
        ck_abort_msg("Could not load dfg");
    }
    return get_dfg();
}


START_TEST(test_get_local_runtime){
    uint32_t ip;
    string_to_ipv4(LOCAL_IP_STR, &ip);
    struct dfg_config *dfg = load_dfg( get_resource_path(DFG_CONFIG_FILE) );
    struct runtime_endpoint *rt = get_local_runtime(dfg, ip);
    ck_assert_msg(rt!=NULL, "Could not find runtime");
    ck_assert_int_eq(rt->ip, ip);
} END_TEST

START_TEST(test_msu_msg_from_vertex){
    mark_point();
    load_dfg( get_resource_path(DFG_CONFIG_FILE) );
    mark_point();

    int msu_id = 1;
    int msu_type = 502;

    struct dfg_vertex *vertex = dfg_msu_from_id(msu_id);
    struct dedos_thread_msg *msg = msu_msg_from_vertex(vertex);
    struct create_msu_thread_msg_data *data = (struct create_msu_thread_msg_data*)msg->data;

    mark_point();
    ck_assert_msg(msg->action == CREATE_MSU, "Improper action type");
    ck_assert_msg(msg->action_data == msu_id, "Improper MSU ID assigned");
    ck_assert_msg(msg->buffer_len == sizeof(*data),
            "Improper buffer length %d (should be %d)",
            msg->buffer_len, sizeof(*data));

    mark_point();
    ck_assert_msg(data->msu_type == msu_type, "Wrong MSU type assigned");
} END_TEST

struct msu_control_add_route * assert_proper_route_message(
        int from_id, int to_id, int to_type, struct dedos_thread_msg *msg){

    mark_point();
    struct msu_control_add_route *data = (struct msu_control_add_route*)msg->data;

    ck_assert_msg(msg->action == MSU_ROUTE_ADD, "Message action not MSU_ROUTE_ADD");
    ck_assert_msg(msg->action_data == from_id, "Improper MSU ID in route add");
    ck_assert_msg(msg->buffer_len == sizeof(*data), "Improper buffer length");
    ck_assert_msg(data->peer_msu_id == to_id, "Wrong destination MSU ID");
    ck_assert_msg(data->peer_msu_type == to_type, "Wrong destination type "
            "(expected %d was %d)", to_type, data->peer_msu_type);
    return data;
}

START_TEST(test_route_msg_from_vertex_local){
    load_dfg( get_resource_path(DFG_CONFIG_FILE) );
    mark_point();

    int from_id = 1;
    int to_id = 2;
    int to_type = 501;

    struct dfg_vertex *from_vertex = dfg_msu_from_id(from_id);
    struct dfg_vertex *to_vertex = dfg_msu_from_id(to_id);
    struct dedos_thread_msg *msg = route_msg_from_vertices(from_vertex, to_vertex);

    struct msu_control_add_route *data = assert_proper_route_message(from_id, to_id, to_type, msg);
    ck_assert_msg(data->peer_locality == 0, "Wrong peer locality for local route "
            "(expected %d, was %d)", 0, data->peer_locality);
    ck_assert_msg(data->peer_ipv4 == 0, "IP address present for local route");
} END_TEST

START_TEST(test_create_msu_from_vertex){
    struct dfg_config *dfg = load_dfg( get_resource_path(DFG_CONFIG_FILE) );
    mark_point();
    int msu_id = 3;

    struct dfg_vertex *vertex = dfg_msu_from_id(msu_id);

    mark_point();
    init_main_thread();
    init_msu_tracker();

    mark_point();
    int rtn = create_msu_from_vertex(vertex);
    ck_assert_msg(rtn < 0, "MSU created despite lack of threads");
    ck_assert_msg(msu_tracker_count() == 0, "MSU created despite lack of threads");

    rtn = spawn_threads_from_dfg(dfg);
    ck_assert_msg(rtn >= 0, "Could not create threads for testing MSU");

    rtn = create_msu_from_vertex(vertex);
    ck_assert_msg(rtn >= 0, "MSU creation returned -1");
    ck_assert_msg(msu_tracker_count() == 1, "MSU creation not tracked");

    struct msu_placement_tracker *tracker = msu_tracker_find(msu_id);
    ck_assert_msg(tracker != NULL, "Tracker could not find placed MSU");
    ck_assert_msg(tracker->msu_id == msu_id, "MSU tracker found wrong MSU");
}END_TEST

START_TEST(test_spawn_threads_from_dfg){
    struct dfg_config *dfg = load_dfg(get_resource_path(DFG_CONFIG_FILE) );
    mark_point();

    init_main_thread();

    int rtn = spawn_threads_from_dfg(dfg);

    ck_assert_msg(rtn >= 0, " return < 0: thread spawn");
    ck_assert_msg(total_threads == 4, "%d threads created, expected 4");
}END_TEST

Suite *implement_dfg_suite(void)
{
    Suite *s = suite_create("DFG_loading");


    TCase *tc_setup = tcase_create("runtime_setup");
    tcase_add_test(tc_setup, test_get_local_runtime);
    suite_add_tcase(s, tc_setup);

    TCase *tc_messages = tcase_create("message_creation");
    tcase_add_test(tc_messages, test_msu_msg_from_vertex);
    tcase_add_test(tc_messages, test_route_msg_from_vertex_local);
    suite_add_tcase(s, tc_messages);

    TCase *tc_resources = tcase_create("resource_creation");
    tcase_add_test(tc_resources, test_spawn_threads_from_dfg);
    tcase_add_test(tc_resources, test_create_msu_from_vertex);
    suite_add_tcase(s, tc_resources);

    return s;
}

int main(int argc, char **argv){
    printf("Argv[0] is %s\n", argv[0]);
    testing_init(argv[0]);
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = implement_dfg_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : -1;
}

