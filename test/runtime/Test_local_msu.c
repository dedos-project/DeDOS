#include "dedos_testing.h"
#include "stats.h"
#include "msu_calls.h"
#include "runtime_dfg.h"

// Include the file under test
#include "local_msu.c"

struct dfg_runtime local_runtime = {
    .id = 1
};

#define msu_id_custom  5
#define msu_id_default 6

static int initted = 0;
static int test_init(struct local_msu *self, struct msu_init_data *init_data) {
    initted = 1;
    return 0;
}

static int destroyed = 0;
static void test_destroy(struct local_msu *self) {
    destroyed = 1;
}

struct msu_msg *data_received = NULL;
static int test_receive(struct local_msu *self, struct msu_msg *data) {
    data_received = data;
    return 0;
}

static int routed = 0;
static int test_route(struct msu_type *type, struct local_msu *sender, 
                      struct msu_msg *data, struct msu_endpoint *output) {
    routed = 1;
    init_msu_endpoint(msu_id_custom, 1, output);
    return 0;
}

static int serialized = 0;
static ssize_t test_serialize(struct msu_type *type, struct msu_msg *input, void **output) {
    serialized = 1;
    return 0;
}

static int deserialized = 0;
static void * test_deserialize(struct local_msu *self, size_t intput_size, void *input, size_t *sz) {
    deserialized = 1;
    return NULL;
}

struct msu_type test_type_custom = {
    .name = "TEST",
    .id = 101,
    .init = test_init,
    .destroy = test_destroy,
    .receive = test_receive,
    .route = test_route,
    .serialize = test_serialize,
    .deserialize = test_deserialize,
};

struct msu_type test_type_default = {
    .name = "TEST_NULLS",
    .id = 102,
    .receive = test_receive
};

struct dedos_thread dthread = {};
struct worker_thread wthread = { .thread = &dthread} ;

struct local_msu msu_custom = {
    .id = msu_id_custom,
    .type = &test_type_custom,
    .thread = &wthread
};

struct local_msu msu_default = {
    .id = msu_id_default,
    .type = &test_type_default,
    .thread = &wthread
};



START_DEDOS_TEST(test_rm_from_local_registry__success) {
    local_msu_registry[msu_id_default] = &msu_default;

    ck_assert_int_eq(rm_from_local_registry(msu_id_default), 0);

    ck_assert_ptr_eq(local_msu_registry[msu_id_default], NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_rm_from_local_registry__fail) {
    ck_assert_int_eq(rm_from_local_registry(5), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_add_to_local_registry__success) {
    ck_assert_int_eq(add_to_local_registry(&msu_default), 0);

    ck_assert_ptr_eq(local_msu_registry[msu_id_default], &msu_default);
} END_DEDOS_TEST

START_DEDOS_TEST(test_add_to_local_registry__fail) {
    local_msu_registry[msu_id_default] = &msu_default;

    ck_assert_int_eq(add_to_local_registry(&msu_default), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_local_msu__success) {
    local_msu_registry[msu_id_default] = &msu_default;

    struct local_msu *msu_out = get_local_msu(msu_id_default);
    ck_assert_ptr_eq(msu_out, &msu_default);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_local_msu__fail) {
    local_msu_registry[msu_id_default] = &msu_default;

    struct local_msu *msu_out = get_local_msu(msu_id_default+100);
    ck_assert_ptr_eq(msu_out, NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_init_msu__custom) {

    struct local_msu *msu = init_msu(msu_id_custom, &test_type_custom, &wthread, NULL);
    ck_assert_int_eq(initted, 1);
    ck_assert_ptr_eq(msu, local_msu_registry[msu_id_custom]);
} END_DEDOS_TEST

START_DEDOS_TEST(test_init_msu__default) {
    struct local_msu *msu = init_msu(msu_id_default, &test_type_default, &wthread, NULL);
    ck_assert_ptr_eq(msu, local_msu_registry[msu_id_default]);
} END_DEDOS_TEST

START_DEDOS_TEST(test_destroy_msu__custom) {
    struct local_msu *msu = init_msu(msu_id_custom, &test_type_custom, &wthread, NULL);
    local_msu_registry[5] = msu;

    destroy_msu(msu);
    ck_assert_int_eq(destroyed, 1);
    ck_assert_ptr_eq(local_msu_registry[5], NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_destroy_msu__default) {
    struct local_msu *msu = init_msu(msu_id_default, &test_type_default, &wthread, NULL);
    local_msu_registry[5] = msu;

    destroy_msu(msu);
    ck_assert_ptr_eq(local_msu_registry[msu_id_default], NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_msu_receive) {
    struct msu_msg data;
    msu_receive(&msu_custom, &data);
    ck_assert_ptr_eq(data_received, &data);
} END_DEDOS_TEST

START_DEDOS_TEST(test_msu_dequeue__message) {
    struct local_msu *msu = init_msu(msu_id_custom, &test_type_custom, &wthread, NULL);

    struct msu_msg_hdr hdr = {
        .key = {
            .key = {123},
        }
    };

    int data = 123;

    struct msu_msg *msg = create_msu_msg(&hdr, sizeof(data), &data);

    enqueue_msu_msg(&msu->queue, msg);

    msu_dequeue(msu);

    // This works even though the pointer has been freed?
    ck_assert_ptr_eq(data_received, msg);
} END_DEDOS_TEST

START_DEDOS_TEST(test_msu_dequeue__empty) {
    struct local_msu *msu = init_msu(msu_id_custom, &test_type_custom, &wthread, NULL);

    msu_dequeue(msu);
    ck_assert_ptr_eq(data_received, NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_msu_enqueue__local) {
    struct local_msu *msu = init_msu(msu_id_default, &test_type_default, &wthread, NULL);
    struct local_msu *msu2 = init_msu(msu_id_custom, &test_type_custom, &wthread, NULL);

    struct msu_msg_hdr hdr = {
        .key = {
            .key = {123},
        }
    };

    int data = 123;

    call_msu_type(msu, &test_type_custom, &hdr, sizeof(data), &data);

    ck_assert_int_eq(msu2->queue.num_msgs, 1);
} END_DEDOS_TEST

// TODO: test_msu_enqueue__remote

DEDOS_START_TEST_LIST("local_msu")

set_local_runtime(&local_runtime);
// Just to keep it from complaining
init_statistics();

DEDOS_ADD_TEST_FN(test_rm_from_local_registry__success)
DEDOS_ADD_TEST_FN(test_rm_from_local_registry__fail)
DEDOS_ADD_TEST_FN(test_add_to_local_registry__success)
DEDOS_ADD_TEST_FN(test_add_to_local_registry__fail)
DEDOS_ADD_TEST_FN(test_get_local_msu__success)
DEDOS_ADD_TEST_FN(test_get_local_msu__fail)
DEDOS_ADD_TEST_FN(test_init_msu__custom)
DEDOS_ADD_TEST_FN(test_init_msu__default)
DEDOS_ADD_TEST_FN(test_destroy_msu__custom)
DEDOS_ADD_TEST_FN(test_destroy_msu__default)
DEDOS_ADD_TEST_FN(test_msu_receive)
DEDOS_ADD_TEST_FN(test_msu_dequeue__message)
DEDOS_ADD_TEST_FN(test_msu_dequeue__empty)
DEDOS_ADD_TEST_FN(test_msu_enqueue__local)

DEDOS_END_TEST_LIST()
