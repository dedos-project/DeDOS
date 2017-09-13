#include "dedos_testing.h"

#include "msu_type.h"


int receive_fn(struct local_msu *self, struct msu_msg *msg) {
    return 0;
}

#define valid_type_id 123

struct msu_type test_type = {
    .id = valid_type_id,
    .receive = receive_fn
};

#define invalid_type_id 456

struct msu_type test_type_invalid = {
    .id = invalid_type_id,
};

int type_initted = 0;
int init_type_fn(struct msu_type *type) {
    type_initted = 1;
    return 0;
}

#define custom_type_id 789

struct msu_type test_type_custom_init = {
    .id = custom_type_id,
    .receive = receive_fn,
    .init_type = init_type_fn
};

#define MSU_TYPE_LIST \
{ \
    &test_type, \
    &test_type_invalid, \
    &test_type_custom_init \
}

// Include the file under test
#include "msu_type.c"


START_DEDOS_TEST(test_register_msu_type) {
    register_msu_type(&test_type);
    ck_assert_ptr_eq(&test_type, msu_types[123]);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_msu_type) {
    msu_types[valid_type_id] = &test_type;
    ck_assert_ptr_eq(get_msu_type(valid_type_id), &test_type);
} END_DEDOS_TEST

START_DEDOS_TEST(test_has_required_fields__pass) {
    ck_assert_int_eq(has_required_fields(&test_type), true);
    ck_assert_int_eq(has_required_fields(&test_type_custom_init), true);
} END_DEDOS_TEST

START_DEDOS_TEST(test_has_required_fields__fail) {
    ck_assert_int_eq(has_required_fields(&test_type_invalid), false);
} END_DEDOS_TEST

START_DEDOS_TEST(test_init_msu_type__default) {
    ck_assert_int_eq(init_msu_type(&test_type), 0);
    ck_assert_ptr_eq(&test_type, msu_types[valid_type_id]);
} END_DEDOS_TEST

START_DEDOS_TEST(test_init_msu_type__custom) {
    ck_assert_int_eq(init_msu_type(&test_type_custom_init), 0);
    ck_assert_ptr_eq(&test_type_custom_init, msu_types[custom_type_id]);
    ck_assert_int_eq(type_initted, 1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_init_msu_type_id__success) {
    ck_assert_int_eq(init_msu_type_id(valid_type_id), 0);
    ck_assert_ptr_eq(msu_types[valid_type_id], &test_type);

    ck_assert_int_eq(init_msu_type_id(custom_type_id), 0);
    ck_assert_ptr_eq(msu_types[custom_type_id], &test_type_custom_init);
} END_DEDOS_TEST

START_DEDOS_TEST(test_init_msu_type_id__fail_invalid_type) {
    ck_assert_int_eq(init_msu_type_id(42), -1);
    ck_assert(msu_types[42] == NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_init_msu_type_id__fail_incomplete_type) {
    ck_assert_int_eq(init_msu_type_id(invalid_type_id), -1);
    ck_assert(msu_types[invalid_type_id] == NULL);
} END_DEDOS_TEST

DEDOS_START_TEST_LIST("MSU_type")

DEDOS_ADD_TEST_FN(test_register_msu_type)
DEDOS_ADD_TEST_FN(test_get_msu_type)
DEDOS_ADD_TEST_FN(test_has_required_fields__pass)
DEDOS_ADD_TEST_FN(test_has_required_fields__fail)
DEDOS_ADD_TEST_FN(test_init_msu_type__default)
DEDOS_ADD_TEST_FN(test_init_msu_type__custom)
DEDOS_ADD_TEST_FN(test_init_msu_type_id__success)
DEDOS_ADD_TEST_FN(test_init_msu_type_id__fail_invalid_type)
DEDOS_ADD_TEST_FN(test_init_msu_type_id__fail_incomplete_type)

DEDOS_END_TEST_LIST()
