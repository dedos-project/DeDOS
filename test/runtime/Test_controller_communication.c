#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "controller_communication.c"

#include <unistd.h>

int init_controller_sock_for_writing() {
    return init_sock_pair(&controller_sock);
}

int init_controller_sock_for_reading() {
    return init_sock_pair(&controller_sock);
}

START_DEDOS_TEST(test_send_init_to_controller) {
    int fd = init_controller_sock_for_writing();

    struct rt_controller_init_msg msg = {
        .runtime_id = 123
    };

    struct rt_controller_msg_hdr hdr = {
        RT_CTL_INIT, sizeof(msg)
    };

    int rtn = send_to_controller(&hdr, &msg);
    ck_assert_int_eq(rtn, 0);

    struct rt_controller_msg_hdr hdr_out;
    struct rt_controller_init_msg msg_out;

    rtn = read(fd, &hdr_out, sizeof(hdr_out));
    ck_assert_int_eq(rtn, sizeof(hdr_out));
    ck_assert_int_eq(hdr_out.type, hdr.type);
    ck_assert_int_eq(hdr_out.payload_size, hdr.payload_size);

    rtn = read(fd, &msg_out, sizeof(msg_out));
    ck_assert_int_eq(rtn, sizeof(msg_out));
    ck_assert_int_eq(msg_out.runtime_id, msg.runtime_id);
} END_DEDOS_TEST

// Just spot-checking a single message type for now
START_DEDOS_TEST(test_thread_msg_from_ctrl_hdr__add_runtime) {

    int fds[2];
    fds[0] = init_sock_pair(&fds[1]);

    struct ctrl_add_runtime_msg msg = {
        .runtime_id = 123,
        .ip = 456,
        .port = 789
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_CONNECT_TO_RUNTIME,
        .thread_id = 0,
        .payload_size = sizeof(msg)
    };

    size_t  written = write(fds[1], &msg, sizeof(msg));
    if (written < 0) {
        log_error("Error writing");
        ck_assert(0);
    }

    struct thread_msg *thread_msg = thread_msg_from_ctrl_hdr(&hdr, fds[0]);

    ck_assert_int_eq(thread_msg->type, CONNECT_TO_RUNTIME);
    ck_assert_int_eq(thread_msg->data_size, sizeof(msg));

    struct ctrl_add_runtime_msg *msg_out = thread_msg->data;

    ck_assert_int_eq(msg_out->runtime_id, msg.runtime_id);
    ck_assert_int_eq(msg_out->ip, msg.ip);
    ck_assert_int_eq(msg_out->port, msg.port);
} END_DEDOS_TEST


DEDOS_START_TEST_LIST("controller_communication")

DEDOS_ADD_TEST_FN(test_send_init_to_controller)
DEDOS_ADD_TEST_FN(test_thread_msg_from_ctrl_hdr__add_runtime)

DEDOS_END_TEST_LIST()
