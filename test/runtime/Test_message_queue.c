#include "dedos_testing.h"

// Include the file under test
#include "message_queue.c"

START_DEDOS_TEST(test_enqueue_and_dequeue_msgs__no_semaphore_no_mutex) {
    struct msg_queue q = {};
    init_msg_queue(&q, NULL);
    struct dedos_msg msg1 = {};
    struct dedos_msg msg2 = {};
    struct dedos_msg msg3 = {};
    enqueue_msg(&q, &msg1);
    enqueue_msg(&q, &msg2);
    enqueue_msg(&q, &msg3);

    struct dedos_msg *out = dequeue_msg(&q);
    ck_assert_ptr_eq(out, &msg1);
    out = dequeue_msg(&q);
    ck_assert_ptr_eq(out, &msg2);
    out = dequeue_msg(&q);
    ck_assert_ptr_eq(out, &msg3);
    out = dequeue_msg(&q);
    ck_assert_ptr_eq(out, NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_enqueue_msg__semaphore) {
    struct msg_queue q;
    sem_t sem;
    struct dedos_msg msg = {};
    sem_init(&sem, 0, 0);
    init_msg_queue(&q, &sem);
    
    ck_assert_int_eq(sem_trywait(&sem), -1);
    enqueue_msg(&q, &msg);
    ck_assert_int_eq(sem_trywait(&sem), 0);
} END_DEDOS_TEST
    

DEDOS_START_TEST_LIST("message_queue")

DEDOS_ADD_TEST_FN(test_enqueue_and_dequeue_msgs__no_semaphore_no_mutex)
DEDOS_ADD_TEST_FN(test_enqueue_msg__semaphore)

DEDOS_END_TEST_LIST()
