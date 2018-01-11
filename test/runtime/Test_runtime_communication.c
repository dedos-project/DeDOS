#include "dedos_testing.h"

// Include the file under test
#include "runtime_communication.c"

// And other dependencies as stated in Test_runtime_communication.mk
#include "local_msu.c"
#include "output_thread.c"

#define local_id 1
struct dfg_runtime local_runtime = {
    .id = local_id
};

#include <signal.h>

int peer_port = 1234;

#define peer_id 2

struct inter_runtime_init_msg msg = {
    .origin_id = local_id
};

struct inter_runtime_msg_hdr hdr = {
    .type = INTER_RT_INIT,
    .target = 0,
    .payload_size = sizeof(msg)
};
START_DEDOS_TEST(test_send_to_peer__success) {
    int read_fd = init_sock_pair(&runtime_peers[peer_id].fd);

    ck_assert_int_eq(send_to_peer(peer_id, &hdr, &msg), 0);

    char read_buf[128];
    ck_assert_int_gt(read(read_fd, read_buf, 128), 0);
}  END_DEDOS_TEST

START_DEDOS_TEST(test_send_to_peer__fail_uninitialized_peer) {
    ck_assert_int_eq(send_to_peer(peer_id, &hdr, &msg), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_send_to_peer__fail_peer_closed) {
    int read_fd = init_sock_pair(&runtime_peers[peer_id].fd);
    close(read_fd);

    signal(SIGPIPE, SIG_IGN);
    ck_assert_int_eq(send_to_peer(peer_id, &hdr, &msg), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_add_runtime_peer__success) {

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    ck_assert_int_eq(add_runtime_peer(peer_id, fd), 0);
    ck_assert_int_eq(runtime_peers[peer_id].fd, fd);
} END_DEDOS_TEST

START_DEDOS_TEST(test_add_runtime_peer__fail_invalid_fd) {
    int fd = 12;

    ck_assert_int_eq(add_runtime_peer(peer_id, fd), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_send_init_msg) {
    log_info("initting");
    int read_fd = init_sock_pair(&runtime_peers[peer_id].fd);
    log_info("Initted");
    ck_assert_int_eq(send_init_msg(peer_id), 0);

    struct inter_runtime_init_msg msg;
    struct inter_runtime_msg_hdr hdr;

    read(read_fd, &hdr, sizeof(hdr));
    read(read_fd, &msg, sizeof(msg));

    ck_assert_int_eq(hdr.type, INTER_RT_INIT);
    ck_assert_int_eq(hdr.target, 0);
    ck_assert_int_eq(hdr.payload_size, sizeof(msg));
    ck_assert_int_eq(msg.origin_id, local_id);
} END_DEDOS_TEST

START_DEDOS_TEST(test_connect_to_runtime_peer__success) {

    int sock = init_test_listening_socket(peer_port);
    struct sockaddr_in addr;
    inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr.s_addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peer_port);

    ck_assert_int_eq(connect_to_runtime_peer(peer_id, &addr), 0);

    ck_assert_int_gt(accept(sock, NULL, NULL), 0);
} END_DEDOS_TEST

START_DEDOS_TEST(test_connect_to_runtime_peer__fail_not_listening) {
    struct sockaddr_in addr;
    inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr.s_addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peer_port);

    ck_assert_int_eq(connect_to_runtime_peer(peer_id, &addr), -1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_process_fwd_to_msu_message) {
    struct msu_type msu_type = {};
    int msu_id = 42;
    struct local_msu msu = {
        .id = msu_id,
        .type = &msu_type
    };
    init_msg_queue(&msu.queue, NULL);

    local_msu_registry[msu_id] = &msu;

    int fds[2];
    fds[0] = init_sock_pair(&fds[1]);

    struct msu_msg msg = {.hdr = {}};

    size_t written_size;
    void *serialized = serialize_msu_msg(&msg, &msu_type, &written_size);
    write(fds[1], serialized, written_size);
    free(serialized);

    log(LOG_TEST, "Wrote %d bytes to fd %d", (int)written_size, fds[1]);
    int rtn = process_fwd_to_msu_message(written_size, msu_id, fds[0]);

    ck_assert_int_eq(rtn, 0);
    ck_assert_int_eq(msu.queue.num_msgs, 1);
    ck_assert_int_eq(msu.queue.head->type, MSU_MSG);
    ck_assert_int_eq(msu.queue.head->data_size, sizeof(msg));

    struct msu_msg *msg_out = dequeue_msu_msg(&msu.queue);
    free(msg_out);

} END_DEDOS_TEST

START_DEDOS_TEST(test_process_init_rt_message) {
    struct inter_runtime_init_msg msg = { .origin_id = peer_id };
    int fds[2];
    fds[0] = init_sock_pair(&fds[1]);

    write(fds[1], &msg, sizeof(msg));

    struct dedos_thread output_thread = {};
    static_output_thread = &output_thread;
    init_msg_queue(&output_thread.queue, NULL);

    int rtn = process_init_rt_message(sizeof(msg), fds[0]);

    ck_assert_int_eq(rtn, 0);

    ck_assert_int_eq(runtime_peers[msg.origin_id].fd, fds[0]);

} END_DEDOS_TEST

START_DEDOS_TEST(test_handle_runtime_communication) {

    struct dedos_thread output_thread = {};
    static_output_thread = &output_thread;
    init_msg_queue(&output_thread.queue, NULL);

    int fd = init_sock_pair(&runtime_peers[peer_id].fd);
    send_to_peer(peer_id, &hdr, &msg);

    int rtn = handle_runtime_communication(fd);

    ck_assert_int_eq(rtn, 0);
    // Should have covered all cases, this is just checking that the
    // reading and parsing of the header goes okay
} END_DEDOS_TEST


DEDOS_START_TEST_LIST("Runtime_communication")

set_local_runtime(&local_runtime);

DEDOS_ADD_TEST_FN(test_send_to_peer__success)
DEDOS_ADD_TEST_FN(test_send_to_peer__fail_uninitialized_peer)
DEDOS_ADD_TEST_FN(test_send_to_peer__fail_peer_closed)
DEDOS_ADD_TEST_FN(test_add_runtime_peer__success)
DEDOS_ADD_TEST_FN(test_add_runtime_peer__fail_invalid_fd)
DEDOS_ADD_TEST_FN(test_send_init_msg)
DEDOS_ADD_TEST_FN(test_connect_to_runtime_peer__success)
DEDOS_ADD_TEST_FN(test_connect_to_runtime_peer__fail_not_listening)
DEDOS_ADD_TEST_FN(test_process_fwd_to_msu_message)
DEDOS_ADD_TEST_FN(test_process_init_rt_message)
DEDOS_ADD_TEST_FN(test_handle_runtime_communication)

DEDOS_END_TEST_LIST()
