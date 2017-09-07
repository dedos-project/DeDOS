#include "dedos_testing.h"

// Include the file under test
#include "local_msu.c"


START_DEDOS_TEST(test_rm_from_local_registry__success) {

    int msu_id = 5;
    struct local_msu *msu = msu_alloc();
    local_msu_registry[msu_id] = msu;

    ck_assert_int_eq(rm_from_local_registry(msu_id), 0);

    ck_assert_ptr_eq(local_msu_registry[msu_id], NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_rm_from_local_registry__fail) {
    ck_assert_int_eq(rm_from_local_registry(5), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_add_to_local_registry__success) {
    int msu_id = 5;
    struct local_msu *msu = msu_alloc();
    msu->id = msu_id;

    ck_assert_int_eq(add_to_local_registry(msu), 0);
    
    ck_assert_ptr_eq(local_msu_registry[msu_id], msu);
} END_DEDOS_TEST

START_DEDOS_TEST(test_add_to_local_registry__fail) {
    int msu_id = 5;
    struct local_msu *msu = msu_alloc();
    msu->id = msu_id;
    local_msu_registry[msu_id] = msu;

    ck_assert_int_eq(add_to_local_registry(msu), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_local_msu__success) {
    int msu_id = 5;
    struct local_msu *msu = msu_alloc();
    msu->id = msu_id;
    local_msu_registry[msu_id] = msu;

    struct local_msu *msu_out = get_local_msu(msu_id);
    ck_assert_ptr_eq(msu_out, msu);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_local_msu__fail) {
    int msu_id = 5;
    struct local_msu *msu = msu_alloc();
    msu->id = msu_id;
    local_msu_registry[msu_id] = msu;

    struct local_msu *msu_out = get_local_msu(msu_id+1);
    ck_assert_ptr_eq(msu_out, NULL);
} END_DEDOS_TEST

DEDOS_START_TEST_LIST("local_msu")

DEDOS_ADD_TEST_FN(test_rm_from_local_registry__success)
DEDOS_ADD_TEST_FN(test_rm_from_local_registry__fail)
DEDOS_ADD_TEST_FN(test_add_to_local_registry__success)
DEDOS_ADD_TEST_FN(test_add_to_local_registry__fail)
DEDOS_ADD_TEST_FN(test_get_local_msu__success)
DEDOS_ADD_TEST_FN(test_get_local_msu__fail)


DEDOS_END_TEST_LIST()
