#define _GNU_SOURCE  // Needed for CPU_SET etc.

#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "dedos_threads.c"

START_DEDOS_TEST(test_get_dedos_thread_fail) {
   struct dedos_thread *thread = get_dedos_thread(3);
   ck_assert(thread == NULL);
} END_DEDOS_TEST

START_DEDOS_TEST(test_get_dedos_thread_success) {
    struct dedos_thread *thread = malloc(sizeof(*thread));
    init_dedos_thread(thread, PINNED_THREAD, 3);
    struct dedos_thread *thread_out = get_dedos_thread(3);
    ck_assert(thread == thread_out);
} END_DEDOS_TEST

int STARTED_THREAD_ID = -1;
enum thread_mode STARTED_THREAD_MODE;

sem_t DESTROYED_THREAD_SEM;

struct test_struct{
    int t;
};

int DESTROYED_THREADS[] = {0, 0, 0};

void *init_fn(struct dedos_thread *thread) {
    STARTED_THREAD_ID = thread->id;
    STARTED_THREAD_MODE = thread->mode;

    struct test_struct *t = malloc(sizeof(*t));
    t->t = thread->id * 10;

    return t;
}

int exec_fn(struct dedos_thread *thread, void *data) {
    struct test_struct *t = data;
    ck_assert_int_eq(t->t, thread->id * 10);
    t->t = thread->id * 15;
    return 0;
}

void destroy_fn(struct dedos_thread *thread, void *data) {
    struct test_struct *t = data;
    ck_assert_int_eq(t->t, thread->id * 15);
    DESTROYED_THREADS[thread->id] = 1;
    sem_post(&DESTROYED_THREAD_SEM);
    free(t);
}

#define TEST_THREAD(th, id) \
    start_dedos_thread(exec_fn, init_fn, destroy_fn, PINNED_THREAD, id, &th); \
    ck_assert_int_eq(STARTED_THREAD_ID, id); \

START_DEDOS_TEST(test_start_dedos_thread) {

    struct dedos_thread thread1;
    struct dedos_thread thread2;
    struct dedos_thread thread3;

    sem_init(&DESTROYED_THREAD_SEM, 0, 0);

    TEST_THREAD(thread1, 0);
    TEST_THREAD(thread2, 1);
    TEST_THREAD(thread3, 2);

    sem_wait(&DESTROYED_THREAD_SEM);
    sem_wait(&DESTROYED_THREAD_SEM);
    sem_wait(&DESTROYED_THREAD_SEM);

    ck_assert_int_eq(DESTROYED_THREADS[0], 1);
    ck_assert_int_eq(DESTROYED_THREADS[1], 1);
    ck_assert_int_eq(DESTROYED_THREADS[2], 1);

} END_DEDOS_TEST

DEDOS_START_TEST_LIST("dedos_threads")

DEDOS_ADD_TEST_FN(test_get_dedos_thread_fail)
DEDOS_ADD_TEST_FN(test_get_dedos_thread_success)
DEDOS_ADD_TEST_FN(test_start_dedos_thread)

DEDOS_END_TEST_LIST()

