#include "dedos_testing.h"

#include "msu_type.h"

#define  test_thread_id 3
#define  msu1_id 12
#define  msu2_id 13
#define  new_msu_id 15
#define msu_type_id 123

int receive_fn(struct local_msu *self, struct msu_msg *msg) {
    return 0;
}

struct msu_type msu_type = {
    .id = msu_type_id,
    .receive = receive_fn
};

#define MSU_TYPE_LIST \
{ \
    &msu_type \
}

#include "msu_type.c"
#include "local_msu.h"

struct local_msu msu1 = {
    .id = msu1_id,
    .type = &msu_type
};

struct local_msu msu2 = {
    .id = msu2_id,
    .type = &msu_type
};

struct dedos_thread dthread = {
    .id = test_thread_id
};

#include "worker_thread.c"

struct worker_thread wthread = {
    .thread = &dthread,
    .n_msus = 2,
    .msus = {&msu1, &msu2}
};


START_DEDOS_TEST(test_init_worker_thread) {
    struct dedos_thread thread = {
        .id = test_thread_id
    };

    struct worker_thread *worker = init_worker_thread(&thread);
    ck_assert_ptr_eq(worker, worker_threads[test_thread_id]);
    ck_assert_ptr_eq(worker->thread, &thread);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_worker_thread) {
    worker_threads[test_thread_id] = &wthread;

    ck_assert_ptr_eq(get_worker_thread(test_thread_id), &wthread);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_msu_index__success) {
    ck_assert_int_eq(get_msu_index(&wthread, msu1_id), 0);
    ck_assert_int_eq(get_msu_index(&wthread, msu2_id), 1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_msu_index__fail) {
    ck_assert_int_eq(get_msu_index(&wthread, 123), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_create_msu_on_thread) {
    init_msu_type_id(msu_type_id);

    struct ctrl_create_msu_msg msg = {
        .msu_id = new_msu_id,
        .type_id = msu_type_id
    };

    ck_assert_int_eq(create_msu_on_thread(&wthread, &msg), 0);
    ck_assert_int_eq(wthread.n_msus, 3);
    ck_assert_int_eq(wthread.msus[2]->id, new_msu_id);
} END_DEDOS_TEST

START_DEDOS_TEST(test_del_msu_from_thread__success) {
    struct ctrl_delete_msu_msg msg = {
        .msu_id = new_msu_id
    };

    struct local_msu *msu3 = calloc(1, sizeof(*msu3));
    msu3->id = new_msu_id;
    msu3->type = &msu_type;

    wthread.msus[wthread.n_msus] = msu3;
    wthread.n_msus++;

    ck_assert_int_eq(del_msu_from_thread(&wthread, &msg, 1), 0);
    ck_assert_int_eq(wthread.n_msus, 2);
    ck_assert_ptr_eq(wthread.msus[2], NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_del_msu_from_thread__fail_no_such_msu) {
    struct ctrl_delete_msu_msg msg = {
        .msu_id = new_msu_id
    };

    ck_assert_int_eq(del_msu_from_thread(&wthread, &msg, 1), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_enqueue_worker_timeouts) {
    struct timespec interval1 = {.tv_sec = 1, .tv_nsec = (1) * 1e7};
    enqueue_worker_timeout(&wthread, &interval1);
    struct timespec interval2 = {.tv_sec = 4, .tv_nsec = (6) * 1e7};
    enqueue_worker_timeout(&wthread, &interval2);
    struct timespec interval3 = {.tv_sec = 2, .tv_nsec = (7) * 1e7};
    enqueue_worker_timeout(&wthread, &interval3);
    struct timespec interval4 = {.tv_sec = 5, .tv_nsec = (1) * 1e7};
    enqueue_worker_timeout(&wthread, &interval4);
    struct timespec interval5 = {.tv_sec = 3, .tv_nsec = (3) * 1e7};
    enqueue_worker_timeout(&wthread, &interval5);

    struct timeout_list *t = wthread.timeouts;
    ck_assert_ptr_ne(t, NULL);
    for (int i=0; i < 4; i++) {
        ck_assert_ptr_ne(t->next, NULL);
        ck_assert_int_gt(t->next->time.tv_sec * 1e9 + t->next->time.tv_nsec,
                         t->time.tv_sec * 1e9 + t->time.tv_nsec);
        t = t->next;
    }
    ck_assert_ptr_eq(t->next, NULL);
} END_DEDOS_TEST

// TODO: worker_thread_mod_msu_route
//       process_worker_thread_msg
//       worker_thread_loop
//       create_worker_thread

DEDOS_START_TEST_LIST("worker_thread")

DEDOS_ADD_TEST_FN(test_init_worker_thread)
DEDOS_ADD_TEST_FN(test_get_worker_thread)
DEDOS_ADD_TEST_FN(test_get_msu_index__success)
DEDOS_ADD_TEST_FN(test_get_msu_index__fail)
DEDOS_ADD_TEST_FN(test_create_msu_on_thread)
DEDOS_ADD_TEST_FN(test_del_msu_from_thread__success)
DEDOS_ADD_TEST_FN(test_del_msu_from_thread__fail_no_such_msu)
DEDOS_ADD_TEST_FN(test_enqueue_worker_timeouts)

DEDOS_END_TEST_LIST()

