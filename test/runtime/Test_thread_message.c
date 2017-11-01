#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "thread_message.c"

// Spot-checking a single thread message type
START_DEDOS_TEST(test_enqueue_and_dequeue_thread_msg) {

    struct send_to_ctrl_msg msg = {};

    struct thread_msg tmsg = {
        .type = CONNECT_TO_RUNTIME,
        .data_size = sizeof(msg),
        .data = &msg
    };

    struct msg_queue q;
    ck_assert_int_eq(init_msg_queue(&q, NULL), 0);

    ck_assert_int_eq(enqueue_thread_msg(&tmsg, &q), 0);

    struct thread_msg *out = dequeue_thread_msg(&q);

    ck_assert_ptr_eq(out, &tmsg);
} END_DEDOS_TEST

START_DEDOS_TEST(test_init_send_thread_msg) {
    int runtime_id = 123;
    int target_id = 12;
    int data = 42;

    struct thread_msg *msg = init_send_thread_msg(runtime_id, target_id, sizeof(data), &data);

    ck_assert_int_eq(msg->type, SEND_TO_PEER);
    ck_assert_int_eq(msg->data_size, sizeof(struct send_to_peer_msg));
    struct send_to_peer_msg *pmsg = msg->data;

    ck_assert_int_eq(pmsg->runtime_id, runtime_id);
    ck_assert_int_eq(pmsg->hdr.type, RT_FWD_TO_MSU);
    ck_assert_int_eq(pmsg->hdr.target, target_id);
    ck_assert_int_eq(pmsg->hdr.payload_size, sizeof(data));
    ck_assert_ptr_eq(pmsg->data, &data);
    free(pmsg);
    free(msg);
} END_DEDOS_TEST



DEDOS_START_TEST_LIST("thread_message")

DEDOS_ADD_TEST_FN(test_enqueue_and_dequeue_thread_msg)
DEDOS_ADD_TEST_FN(test_init_send_thread_msg)

DEDOS_END_TEST_LIST()
