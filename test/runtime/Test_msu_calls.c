#include "dedos_testing.h"

#include "msu_calls.c"
#include "worker_thread.c"

struct dedos_thread dthread = {};
struct worker_thread wthread = { .thread = &dthread };
struct msu_type type = {};
struct local_msu msu = {
    .type = &type,
    .thread = &wthread
};

struct msu_msg_key key = {};

START_DEDOS_TEST(test_schedule_local_msu_init_call) {
    // In how long should the message be delivered
    struct timespec second = {
        .tv_sec = 0,
        .tv_nsec = 1e8
    };

    // This is the actual call that delivers the message
    int rtn = schedule_local_msu_init_call(&msu, &msu, &second, &key, 0, NULL);
    ck_assert_int_eq(rtn, 0);

    // If we try to dequeue now, it shouldn't work, message should be NULL
    struct msu_msg *msg = dequeue_msu_msg(&msu.queue);
    ck_assert_ptr_eq(msg, NULL);
    free(msg);

    // Then we wait until the next timeout
    rtn = thread_wait(wthread.thread, next_timeout(&wthread));
    ck_assert_int_eq(rtn, 0);

    // This time when we dequeue, the message should be there
    msg = dequeue_msu_msg(&msu.queue);
    ck_assert_ptr_ne(msg, NULL);
    free(msg);
} END_DEDOS_TEST

DEDOS_START_TEST_LIST("MSU calls");

DEDOS_ADD_TEST_FN(test_schedule_local_msu_init_call)

DEDOS_END_TEST_LIST()




