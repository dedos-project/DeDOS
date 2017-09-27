#include "dedos_testing.h"

// Include the file under test
#include "main_thread.c"

// Include others referenced from Test_main_thread.mk
#include "runtime_dfg.c"
#include "runtime_communication.c"

#include "communication.h"

struct dfg_runtime local_runtime = {
    .id = 1
};

unsigned int thread_id = 2;

START_DEDOS_TEST(test_add_worker_thread) {
    struct ctrl_create_thread_msg msg = {
        .thread_id = thread_id,
        .mode = PINNED_THREAD
    };

    ck_assert_int_eq(add_worker_thread(&msg), 0);

    struct dedos_thread *created = get_dedos_thread(thread_id);

    ck_assert_int_eq(created->id, thread_id);
    ck_assert_int_eq(created->mode, PINNED_THREAD);
} END_DEDOS_TEST

static int port = 1234;
int fake_listening_runtime() {
    int sock = init_listening_socket(port);
    log(TEST, "Initialized listening socket %d on port %d", sock, port);
    ck_assert_int_gt(sock, 0);
    return sock;
}

START_DEDOS_TEST(test_main_thread_connect_to_runtime) {
    int sock =  fake_listening_runtime();
    struct ctrl_add_runtime_msg msg = {
        .runtime_id = 2,
        .port = port
    };
    inet_pton(AF_INET, "0.0.0.0", &msg.ip);

    ck_assert_int_eq(main_thread_connect_to_runtime(&msg), 0);
    int accepted = accept(sock, NULL, NULL);

    ck_assert_int_gt(accepted, 0);
} END_DEDOS_TEST

START_DEDOS_TEST(test_main_thread_add_connected_runtime) {

    int runtime_id = 2;
    int runtime_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct runtime_connected_msg msg = {
        .runtime_id = runtime_id,
        .fd = runtime_fd
    };

    ck_assert_int_eq(main_thread_add_connected_runtime(&msg), 0);

    ck_assert_int_eq(runtime_peers[runtime_id].fd, runtime_fd);

} END_DEDOS_TEST

START_DEDOS_TEST(test_main_thread_send_to_peer__success) {
    int pipe_fds[2];
    pipe(pipe_fds);

    int remote_runtime_id = 2;
    runtime_peers[remote_runtime_id].fd = pipe_fds[1];

    struct send_to_peer_msg msg = {
        .runtime_id = remote_runtime_id,
        .hdr = {},
        .data = NULL
    };

    char out_buf[16];
    ck_assert_int_eq(main_thread_send_to_peer(&msg), 0);
    ck_assert_int_gt(read(pipe_fds[0], out_buf, 16), 0);
} END_DEDOS_TEST

START_DEDOS_TEST(test_main_thread_send_to_peer__fail_nonexistent_peer) {
    struct send_to_peer_msg msg = {
        .runtime_id = 2,
        .hdr = {},
        .data = NULL
    };
    ck_assert_int_eq(main_thread_send_to_peer(&msg), -1);
} END_DEDOS_TEST

// TODO (maybe): main_thread_control_route,
//               process_main_thread_msg
//               check_main_thread_queue
//               main_thread_loop

START_DEDOS_TEST(test_enqueue_to_main_thread) {
    struct dedos_thread main_thread = {};
    init_msg_queue(&main_thread.queue, NULL);
    static_main_thread = &main_thread;
    struct thread_msg msg = {};
    ck_assert_int_eq(enqueue_to_main_thread(&msg), 0);
    ck_assert_int_eq(static_main_thread->queue.num_msgs, 1);
    ck_assert_ptr_eq(static_main_thread->queue.head->data, &msg);
} END_DEDOS_TEST

DEDOS_START_TEST_LIST("main_thread")

LOCAL_RUNTIME = &local_runtime;

DEDOS_ADD_TEST_FN(test_add_worker_thread)
DEDOS_ADD_TEST_FN(test_main_thread_connect_to_runtime)
DEDOS_ADD_TEST_FN(test_main_thread_add_connected_runtime)
DEDOS_ADD_TEST_FN(test_main_thread_send_to_peer__success)
DEDOS_ADD_TEST_FN(test_main_thread_send_to_peer__fail_nonexistent_peer)
DEDOS_ADD_TEST_FN(test_enqueue_to_main_thread)

DEDOS_END_TEST_LIST()
