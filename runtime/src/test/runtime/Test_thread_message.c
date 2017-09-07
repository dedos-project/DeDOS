#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "thread_message.c"

// Spot-checking a single thread message type
START_TEST(test_enqueue_and_dequeue_thread_msg) {

    struct runtime_connected_msg msg = {
        .runtime_id = 123,
        .fd = 456
    };

    struct thread_msg tmsg = {
        .type = RUNTIME_CONNECTED,
        .data_size = sizeof(msg),
        .data = &msg
    };

    struct msg_queue q;
    ck_assert_int_eq(init_msg_queue(&q, NULL), 0);
        
    ck_assert_int_eq(enqueue_thread_msg(&tmsg, &q), 0);

    struct thread_msg *out = dequeue_thread_msg(&q);
    
    ck_assert_ptr_eq(out, &tmsg);
} END_TEST

DEDOS_START_TEST_LIST("thread_message")

DEDOS_ADD_TEST_FN(test_enqueue_and_dequeue_thread_msg)

DEDOS_END_TEST_LIST()
