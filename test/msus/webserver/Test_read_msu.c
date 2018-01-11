#include "dedos_testing.h"
#include "routing_test_utils.h"
#include "socket_msu.h"

#define MONITOR_NUM_FDS

// Include file under test
#include "webserver/read_msu.c"
#include "socket_msu.c"

#include <unistd.h>
#include <fcntl.h>

#define LOCAL_RT_ID 1
#define READ_MSU_ID 2
#define HTTP_MSU_ID 3
#define SOCKET_MSU_ID 4

struct local_msu http_msu = {
    .type = &WEBSERVER_HTTP_MSU_TYPE,
    .id = HTTP_MSU_ID
};

struct sock_msu_state sock_state = {};

struct local_msu sock_msu = {
    .type = &SOCKET_MSU_TYPE,
    .id = SOCKET_MSU_ID,
    .msu_state = &sock_state
};

struct local_msu *instance = &sock_msu;
START_DEDOS_TEST(test_read_http_request__from_sock_no_ssl__one_shot) {

    struct ws_read_state state = {
        .use_ssl = 0
    };

    struct local_msu read_msu = {
        .type = &WEBSERVER_READ_MSU_TYPE,
        .id = READ_MSU_ID,
        .msu_state = &state
    };

    INIT_LOCAL_RUNTIME(LOCAL_RT_ID);

    ADD_ROUTE_TO_MSU(read_msu, http_msu, LOCAL_RT_ID, 12);

    int fd_out;
    int fd_in = init_sock_pair(&fd_out);

    struct socket_msg *smsg = malloc(sizeof(*smsg));
    smsg->fd = fd_in;

    struct msu_msg_key key = {
        .key = {1},
        .key_len = sizeof(int),
        .id = 1
    };

    struct msu_msg msg = {
        .hdr = {.key = key},
        .data = smsg
    };

    add_provinance(&msg.hdr.provinance, &sock_msu);

    write(fd_out, "HELLO!", strlen("HELLO!"));
    int rtn = read_http_request(&read_msu, &msg);

    ck_assert_int_eq(rtn, 0);
    ck_assert_int_eq(http_msu.queue.num_msgs, 1);
    struct msu_msg *msg_out = dequeue_msu_msg(&http_msu.queue);

    struct read_state *read_state = msg_out->data;
    ck_assert_str_eq(read_state->req, "HELLO!");
    free(read_state);
    free(msg_out);
    close(fd_in);
    close(fd_out);
    FREE_MSU_ROUTES(read_msu);
} END_DEDOS_TEST

START_DEDOS_TEST(test_read_http_request__from_sock_no_ssl__partial_read_and_second_shot) {
    struct ws_read_state state = {
        .use_ssl = 0
    };

    struct local_msu read_msu = {
        .type = &WEBSERVER_READ_MSU_TYPE,
        .id = READ_MSU_ID,
        .msu_state = &state
    };

    init_statistics();
    init_stat_item(MSU_STAT1, sock_msu.id);
    INIT_LOCAL_RUNTIME(LOCAL_RT_ID);
    ADD_ROUTE_TO_MSU(read_msu, http_msu, LOCAL_RT_ID, 12);

    int fd_out;
    int fd_in = init_sock_pair(&fd_out);

    fcntl(fd_in, F_SETFL, O_NONBLOCK);

    struct socket_msg *smsg = malloc(sizeof(*smsg));
    smsg->fd = fd_in;

    struct msu_msg_key key = {
        .key = {1},
        .key_len = sizeof(int),
        .id = 1
    };

    struct msu_msg msg = {
        .hdr = {.key = key},
        .data= smsg
    };

    add_provinance(&msg.hdr.provinance, &sock_msu);

    int rtn = read_http_request(&read_msu, &msg);

    ck_assert_int_eq(rtn, 0);
    ck_assert_int_eq(http_msu.queue.num_msgs, 0);

    int n_fds = (int)get_last_stat(MSU_STAT1, sock_msu.id);

    ck_assert_int_eq(n_fds, 1);
    // Assert socket msu stored the reference
    ck_assert_ptr_eq(sock_state.destinations[fd_in], &read_msu);
    ck_assert_int_eq(msu_num_states(&read_msu), 1);

    write(fd_out, "HELLO!", strlen("HELLO!"));
    smsg = malloc(sizeof(*smsg));
    smsg->fd = fd_in;

    msg.data = smsg;

    rtn = read_http_request(&read_msu, &msg);

    ck_assert_int_eq(rtn, 0);
    ck_assert_int_eq(http_msu.queue.num_msgs, 1);
    FREE_MSU_ROUTES(read_msu);

} END_DEDOS_TEST


DEDOS_START_TEST_LIST("read_msu")

DEDOS_ADD_TEST_FN(test_read_http_request__from_sock_no_ssl__one_shot);
DEDOS_ADD_TEST_FN(test_read_http_request__from_sock_no_ssl__partial_read_and_second_shot);

DEDOS_END_TEST_LIST()
