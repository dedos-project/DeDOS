#include "dedos_testing.h"

// Wheeee!
// (Need static methods...)
#include "controller_communication.c"

#include <unistd.h>

int init_controller_sock_for_writing() {
    int pipe_fd[2];
    int rtn = pipe(pipe_fd);
    if (rtn < 0) {
        log_critical("Could not open pipe");
        return -1;
    }
    controller_sock = pipe_fd[1];
    return pipe_fd[0];
}

int init_controller_sock_for_reading() {
    int pipe_fd[2];
    int rtn = pipe(pipe_fd);
    if (rtn < 0) {
        log_critical("Could not open pipe");
        return -1;
    }
    controller_sock = pipe_fd[0];
    return pipe_fd[1];
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
    int pipe_fd[2];
    pipe(pipe_fd);

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

    write(pipe_fd[1], &msg, sizeof(msg));

    struct thread_msg *thread_msg = thread_msg_from_ctrl_hdr(&hdr, pipe_fd[0]);

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
