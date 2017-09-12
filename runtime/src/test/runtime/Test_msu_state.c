#include "dedos_testing.h"

#include "msu_state.c"

struct local_msu msu = {
    .id = 1,
    .item_state = NULL
};

struct test_a {
    int a;
    int b;
};

struct test_b {
    int a;
};

struct msu_msg_key key1 = {
    .key = {
        .k1 = 123,
        .k2 = 456,
        .k3 = 789
    },
    .key_len = sizeof(struct composite_key),
    .id = 123456789
};

struct msu_msg_key key2 = {
    .key = {
        .k1 = 123
    },
    .key_len = sizeof(uint64_t),
    .id = 123
};

START_DEDOS_TEST(test_msu_init_state) {

    struct test_a *data = msu_init_state(&msu, &key1, sizeof(*data));
    struct test_b *data2 = msu_init_state(&msu, &key2, sizeof(*data2));
        
    struct msu_state *state;
    HASH_FIND(hh, msu.item_state, &key1.key, key1.key_len, state);
    ck_assert_ptr_eq(data, state->data);
    HASH_FIND(hh, msu.item_state, &key2.key, key2.key_len, state);
    ck_assert_ptr_eq(data2, state->data);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_state__error_nonexistent) {

    struct test_a *data = msu_init_state(&msu, &key1, sizeof(*data));

    struct test_a *data_out = msu_get_state(&msu, &key2, NULL);
    ck_assert_ptr_eq(data_out, NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_state__success) {
    struct test_a *data = msu_init_state(&msu, &key1, sizeof(*data));
    struct test_b *data2 = msu_init_state(&msu, &key2, sizeof(*data2));

    size_t size_out;
    struct test_a *data_out = msu_get_state(&msu, &key1, &size_out);
    ck_assert_ptr_eq(data_out, data);
    ck_assert_int_eq(size_out, sizeof(*data));

    struct test_b *data_out_2 = msu_get_state(&msu, &key2, NULL);
    ck_assert_ptr_eq(data_out_2, data2);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_freed_state) {
    struct test_a *data = msu_init_state(&msu, &key1, sizeof(*data));

    struct test_a *data_out = msu_get_state(&msu, &key1, NULL);
    ck_assert_ptr_eq(data_out, data);

    int rtn = msu_free_state(&msu, &key1);
    ck_assert_int_eq(rtn, 0);
    data_out = msu_get_state(&msu, &key1, NULL);
    ck_assert_ptr_eq(data_out, NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_free_state__fail_nonexistent) {
    int rtn = msu_free_state(&msu, &key1);
    ck_assert_ptr_eq(rtn, -1);
} END_DEDOS_TEST


DEDOS_START_TEST_LIST("Msu_state")

DEDOS_ADD_TEST_FN(test_msu_init_state)
DEDOS_ADD_TEST_FN(test_get_state__error_nonexistent)
DEDOS_ADD_TEST_FN(test_get_state__success)
DEDOS_ADD_TEST_FN(test_get_freed_state) 
DEDOS_ADD_TEST_FN(test_free_state__fail_nonexistent)

DEDOS_END_TEST_LIST()


