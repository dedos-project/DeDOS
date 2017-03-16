#include "routing.h"
#include "global_controller/dfg.h"
#include "dfg_interpreter.h"
#include "testing_utils.h"
#include "routing.h"
#include <stdio.h>
#include <check.h>

// Forward declarations of functions under test:
struct runtime_endpoint *get_local_runtime(struct dfg_config *dfg, uint32_t local_ip);
int create_vertex_routes(struct dfg_vertex *vertex);
int create_route_from_vertices(struct dfg_vertex *from, struct dfg_vertex *to);
struct dedos_thread_msg *route_msg_from_vertices(struct dfg_vertex *from,
                                                 struct dfg_vertex *to);
int create_runtime_threads(struct runtime_endpoint *rt);
struct runtime_endpoint *get_local_runtime(struct dfg_config *dfg, uint32_t local_ip);
int create_msu_from_vertex(struct dfg_vertex *vertex, int max_threads);
struct dedos_thread_msg *msu_msg_from_vertex(struct dfg_vertex *vertex);



// Pretending local address is 158.130.57.63
#define MK_IP(a, b, c, d) \
    ((uint32_t)(a<<24)) + ((uint32_t)(b<<16)) + ((uint32_t)(c<<8)) + ((uint32_t)(d))

#define LOCAL_IP_STR "158.130.57.63"
#define DFG_CONFIG_FILE "sample_dfg.json"

struct dfg_config *load_dfg(char *dfg_path){
    if (dfg_path == NULL)
        ck_abort_msg("No DFG file to load");
    int rtn = do_dfg_config(dfg_path);
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
    load_dfg( get_resource_path(DFG_CONFIG_FILE) );
    int msu_id = 1;
    struct dfg_vertex *vertex = dfg_msu_from_id(msu_id);
    struct dedos_thread_msg *msg = msu_msg_from_vertex(vertex);

    ck_assert_msg(msg->action == CREATE_MSU, "Improper action type");
    ck_assert_msg(msg->action_data == msu_id, "Improper MSU ID assigned");
    ck_assert_msg(msg->buffer_len == sizeof(*(msg->data)), "Improper buffer size");

    struct create_msu_thread_msg_data *data = (struct create_msu_thread_msg_data*)msg->data;
    ck_assert_msg(data->msu_type == 502, "Wrong MSU ID assigned");
} END_TEST


Suite *implement_dfg_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("DFG_loading");

    tc_core = tcase_create("runtime_setup");

    tcase_add_test(tc_core, test_get_local_runtime);
    tcase_add_test(tc_core, test_msu_msg_from_vertex);
    suite_add_tcase(s, tc_core);

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

