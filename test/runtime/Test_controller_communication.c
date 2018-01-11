#define _GNU_SOURCE
#include "dedos_testing.h"
#include "socket_msu.h"

// Wheeee!
// (Need static methods...)
#include "controller_communication.c"
#include "runtime_communication.c"
#include "dedos_threads.c"

#include <unistd.h>

#define RT_ID 5
#define RT_IP 12345
#define RT_PORT 1234

static struct dfg_runtime rt = {
    .id = RT_ID,
    .ip = RT_IP,
    .port = RT_PORT
};

static void init_local_runtime() {
    set_local_runtime(&rt);
}

static void confirm_received_ctl_init_msg(int fd) {

    struct rt_controller_msg_hdr hdr;
    struct rt_controller_init_msg msg;

    int rtn = read(fd, &hdr, sizeof(hdr));
    ck_assert_int_eq(rtn, sizeof(hdr));

    ck_assert_int_eq(hdr.type, RT_CTL_INIT);
    ck_assert_int_eq(hdr.payload_size, sizeof(msg));

    rtn = read(fd, &msg, sizeof(msg));
    ck_assert_int_eq(msg.runtime_id, rt.id);
    ck_assert_int_eq(msg.ip, rt.ip);
    ck_assert_int_eq(msg.port, rt.port);
}

START_DEDOS_TEST(test_send_ctl_init_msg) {
    init_local_runtime();
    int fd = init_sock_pair(&controller_sock);

    int rtn = send_ctl_init_msg();
    ck_assert_int_eq(rtn, 0);

    confirm_received_ctl_init_msg(fd);

    close(fd);
    close(controller_sock);
} END_DEDOS_TEST

#define LISTENING_PORT 9876

START_DEDOS_TEST(test_connect_to_controller) {
    init_local_runtime();
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(LISTENING_PORT)
    };

    int sock = init_test_listening_socket(LISTENING_PORT);

    int ctrl_sock = connect_to_controller(&addr);

    ck_assert_int_gt(ctrl_sock, 0);

    int fd = accept(sock, NULL, NULL);

    confirm_received_ctl_init_msg(fd);

    close(fd);
    close(controller_sock);
} END_DEDOS_TEST

#define ADD_RT_ID 3
#define ADD_RT_IP 0

static void assert_received_rt_init_msg(int fd) {
    struct inter_runtime_msg_hdr hdr;
    struct inter_runtime_init_msg msg;

    int rtn = read(fd, &hdr, sizeof(hdr));
    ck_assert_int_eq(rtn, sizeof(hdr));

    ck_assert_int_eq(hdr.type, INTER_RT_INIT);
    ck_assert_int_eq(hdr.payload_size, sizeof(msg));

    rtn = read(fd, &msg, sizeof(msg));
    ck_assert_int_eq(msg.origin_id, rt.id);
}

START_DEDOS_TEST(test_process_connect_to_runtime) {
    init_local_runtime();
    int sock = init_test_listening_socket(LISTENING_PORT);
    struct ctrl_add_runtime_msg msg = {
        .runtime_id =  ADD_RT_ID,
        .ip = ADD_RT_IP,
        .port = LISTENING_PORT
    };
    int rtn = process_connect_to_runtime(&msg);

    ck_assert_int_eq(rtn, 0);
    int fd = accept(sock, NULL, NULL);

    assert_received_rt_init_msg(fd);

    close(fd);
    close(sock);
} END_DEDOS_TEST

#define THREAD_ID 9
#define THREAD_MODE UNPINNED_THREAD

static void cleanup_worker(int id) {
    struct worker_thread *thread = get_worker_thread(id);
    struct dedos_thread *d_thread = thread->thread;
    dedos_thread_stop(d_thread);
    dedos_thread_join(d_thread);
}

START_DEDOS_TEST(test_process_create_thread_msg__success) {
    struct ctrl_create_thread_msg msg = {
        .thread_id = THREAD_ID,
        .mode = THREAD_MODE
    };

    int rtn = process_create_thread_msg(&msg);
    ck_assert_int_eq(rtn, 0);

    struct worker_thread *thread = get_worker_thread(THREAD_ID);
    ck_assert_ptr_ne(thread, NULL);
    ck_assert_int_eq(thread->thread->id, THREAD_ID);

    cleanup_worker(THREAD_ID);

} END_DEDOS_TEST

START_DEDOS_TEST(test_process_create_thread_msg__fail_already_exists) {
    struct ctrl_create_thread_msg msg = {
        .thread_id = THREAD_ID,
        .mode = THREAD_MODE
    };

    create_worker_thread(THREAD_ID, THREAD_MODE);

    int rtn = process_create_thread_msg(&msg);
    ck_assert_int_eq(rtn, -1);

    cleanup_worker(THREAD_ID);

} END_DEDOS_TEST

#define TEST_ROUTE_ID 123

START_DEDOS_TEST(test_process_ctrl_route_msg__creation_success) {
    struct ctrl_route_msg msg = {
        .type = CREATE_ROUTE,
        .route_id = TEST_ROUTE_ID,
        .type_id = SOCKET_MSU_TYPE_ID
    };

    int rtn = process_ctrl_route_msg(&msg);
    ck_assert_int_eq(rtn, 0);

    struct route_set set = {};
    rtn = add_route_to_set(&set, TEST_ROUTE_ID);
    ck_assert_int_eq(rtn, 0);
    free(set.routes);
} END_DEDOS_TEST

START_DEDOS_TEST(test_process_ctrl_route_msg__creation_fail_exists) {

    int rtn = init_route(TEST_ROUTE_ID, SOCKET_MSU_TYPE_ID);

    ck_assert_int_eq(rtn, 0);

    struct ctrl_route_msg msg = {
        .type= CREATE_ROUTE,
        .route_id = TEST_ROUTE_ID,
        .type_id = SOCKET_MSU_TYPE_ID
    };
    
    rtn = process_ctrl_route_msg(&msg);
    ck_assert_int_ne(rtn, 0);
} END_DEDOS_TEST

#define TEST_MSU_ID 12
#define TEST_ROUTE_KEY 2

START_DEDOS_TEST(test_process_ctrl_route_msg__addition_fail_nonexistent_msu) {
    init_local_runtime();
    int rtn = init_route(TEST_ROUTE_ID, SOCKET_MSU_TYPE_ID);

    ck_assert_int_eq(rtn, 0);

    struct ctrl_route_msg msg = {
        .type = ADD_ENDPOINT,
        .route_id = TEST_ROUTE_ID,
        .msu_id = TEST_MSU_ID,
        .key = TEST_ROUTE_KEY,
        .runtime_id = RT_ID
    };


    rtn = process_ctrl_route_msg(&msg);
    ck_assert_int_ne(rtn, 0);
} END_DEDOS_TEST

START_DEDOS_TEST(test_process_ctrl_route_msg__addition_success_remote_msu) {
    init_local_runtime();
    int rtn = init_route(TEST_ROUTE_ID, SOCKET_MSU_TYPE_ID);

    ck_assert_int_eq(rtn, 0);

    struct ctrl_route_msg msg = {
        .type = ADD_ENDPOINT,
        .route_id = TEST_ROUTE_ID,
        .msu_id = TEST_MSU_ID,
        .key = TEST_ROUTE_KEY,
        .runtime_id = RT_ID + 1
    };


    rtn = process_ctrl_route_msg(&msg);
    ck_assert_int_eq(rtn, 0);
} END_DEDOS_TEST

#define TEST_THREAD_ID 1

START_DEDOS_TEST(test_pass_ctrl_msg_to_thread__success) {
    struct dedos_thread thread = {
        .id = TEST_THREAD_ID,
    };
    dedos_threads[TEST_THREAD_ID] = &thread;

    struct ctrl_create_msu_msg msg = {
        .msu_id = 4321,
        .type_id = 1234
    };
    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_CREATE_MSU,
        .id = -1,
        .thread_id = TEST_THREAD_ID,
        .payload_size = sizeof(msg)
    };

    int fds[2];
    fds[0] = init_sock_pair(&fds[1]);
    write(fds[0], &msg, sizeof(msg));

    int rtn = pass_ctrl_msg_to_thread(&hdr, fds[1]);
    ck_assert_int_eq(rtn, 0);
    ck_assert_int_eq(thread.queue.num_msgs, 1);

    struct thread_msg *tmsg = dequeue_thread_msg(&thread.queue);
    struct ctrl_create_msu_msg *msg_out = tmsg->data;
    ck_assert_int_eq(msg_out->msu_id, 4321);
    ck_assert_int_eq(msg_out->type_id, 1234);

    free(tmsg);
    free(msg_out);
    close(fds[0]);
    close(fds[1]);
} END_DEDOS_TEST


// TESTME: pass_ctrl_msg_to_thread()
// TESTME: thread_msg_from_ctrl_hdr()

// Just spot-checking a single message type for now
START_DEDOS_TEST(test_thread_msg_from_ctrl_hdr__create_msu) {

    int fds[2];
    fds[0] = init_sock_pair(&fds[1]);

    struct ctrl_create_msu_msg msg = {
        .msu_id = 123,
        .type_id = 456,
        .init_data = {
            .init_data = "SUP?"
        }
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_CREATE_MSU,
        .thread_id = 1,
        .payload_size = sizeof(msg)
    };

    size_t  written = write(fds[1], &msg, sizeof(msg));
    if (written < 0) {
        log_error("Error writing");
        ck_assert(0);
    }

    struct thread_msg *thread_msg = thread_msg_from_ctrl_hdr(&hdr, fds[0]);

    ck_assert_int_eq(thread_msg->type, CREATE_MSU);
    ck_assert_int_eq(thread_msg->data_size, sizeof(msg));

    struct ctrl_create_msu_msg *msg_out = thread_msg->data;

    ck_assert_int_eq(msg_out->msu_id, msg.msu_id);
    ck_assert_int_eq(msg_out->type_id, msg.type_id);
    ck_assert_str_eq(msg_out->init_data.init_data, msg.init_data.init_data);

    free(thread_msg);
    free(msg_out);
} END_DEDOS_TEST

// TESTME: process_ctrl_message_hdr()

// TESTME: send_stats_to_controller()

DEDOS_START_TEST_LIST("controller_communication")

DEDOS_ADD_TEST_FN(test_send_ctl_init_msg)
DEDOS_ADD_TEST_FN(test_connect_to_controller)
DEDOS_ADD_TEST_FN(test_process_connect_to_runtime)

DEDOS_ADD_TEST_FN(test_process_create_thread_msg__success)
DEDOS_ADD_TEST_FN(test_process_create_thread_msg__fail_already_exists)
DEDOS_ADD_TEST_FN(test_process_ctrl_route_msg__addition_fail_nonexistent_msu);
DEDOS_ADD_TEST_FN(test_process_ctrl_route_msg__addition_success_remote_msu);

DEDOS_ADD_TEST_FN(test_pass_ctrl_msg_to_thread__success);

DEDOS_ADD_TEST_FN(test_process_ctrl_route_msg__creation_success)
DEDOS_ADD_TEST_FN(test_process_ctrl_route_msg__creation_fail_exists)
DEDOS_ADD_TEST_FN(test_thread_msg_from_ctrl_hdr__create_msu)

DEDOS_END_TEST_LIST()
