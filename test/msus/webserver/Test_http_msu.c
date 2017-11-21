#include "dedos_testing.h"
#include "routing_test_utils.h"

#define MONITOR_NUM_FDS

// Include file under test
#include "webserver/http_msu.c"
// So we can set the file size smaller, enabling memcheck to work more quickly
#define MAX_FILE_SIZE 16384
#include "webserver/dbops.c"
#include "socket_msu.c"

#define DB_PORT 9876
#define DB_REQUEST "/database"

#define HTTP_MSU_ID 2
#define READ_MSU_ID 3
#define SOCKET_MSU_ID 5
#define WRITE_MSU_ID 4
#define LOCAL_RT_ID 1
#define READ_ROUTE_ID 123

struct sock_msu_state sock_state = {};

struct local_msu socket_msu = {
    .type = &SOCKET_MSU_TYPE,
    .id = SOCKET_MSU_ID,
    .msu_state = &sock_state
};

struct local_msu *instance  = &socket_msu;

struct local_msu read_msu = {
    .type = &WEBSERVER_READ_MSU_TYPE,
    .id = READ_MSU_ID
};

struct local_msu write_msu = {
    .type = &WEBSERVER_WRITE_MSU_TYPE,
    .id = WRITE_MSU_ID
};


START_DEDOS_TEST(test_craft_http_response__return_to_read__incomplete) {
    struct local_msu http_msu = {
        .type = &WEBSERVER_HTTP_MSU_TYPE,
        .id = HTTP_MSU_ID,
    };

    INIT_LOCAL_RUNTIME(LOCAL_RT_ID);

    ADD_ROUTE_TO_MSU(http_msu, read_msu, LOCAL_RT_ID, 12);

    struct read_state read_state = {
        .req = "",
        .req_len = 0
    };

    struct msu_msg_key key = {
        .key = {1},
        .key_len = sizeof(int),
        .id = 1
    };

    struct msu_msg msg = {
        .hdr = {.key = key},
        .data = &read_state,
        .data_size = sizeof(read_state)
    };

    add_provinance(&msg.hdr.provinance, &read_msu);

    int rtn = craft_http_response(&http_msu, &msg);

    ck_assert_int_eq(rtn, 0);
    ck_assert_int_eq(read_msu.queue.num_msgs, 1);
    ck_assert_int_eq(msu_num_states(&http_msu), 1);
    struct msu_msg *msg_out = dequeue_msu_msg(&read_msu.queue);
    ck_assert_ptr_eq(&read_state, msg_out->data);

    free(msg_out);
    msu_free_state(&http_msu, &key);
    FREE_MSU_ROUTES(http_msu);
} END_DEDOS_TEST

START_DEDOS_TEST(test_craft_http_response__send_error_to_write) {

    struct local_msu http_msu = {
        .type = &WEBSERVER_HTTP_MSU_TYPE,
        .id = HTTP_MSU_ID,
    };

    INIT_LOCAL_RUNTIME(LOCAL_RT_ID);

    ADD_ROUTE_TO_MSU(http_msu, write_msu, LOCAL_RT_ID, 13);

    struct read_state read_state = {
        .req = "I AM NOT HTTP",
        .req_len = 13
    };

    struct msu_msg msg = {
        .data = &read_state,
        .data_size = sizeof(read_state)
    };

    add_provinance(&msg.hdr.provinance, &read_msu);

    int rtn = craft_http_response(&http_msu, &msg);

    ck_assert_int_eq(rtn, 0);
    ck_assert_int_eq(write_msu.queue.num_msgs, 1);
    struct msu_msg *msg_out = dequeue_msu_msg(&write_msu.queue);
    ck_assert_ptr_eq(&read_state, msg_out->data);

    free(msg_out);
    FREE_MSU_ROUTES(http_msu);
} END_DEDOS_TEST

#define REQ_STR "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: dedos\r\n\r\n"

START_DEDOS_TEST(test_craft_http_response__valid_http__no_db_no_regex) {

    struct local_msu http_msu = {
        .type = &WEBSERVER_HTTP_MSU_TYPE,
        .id = HTTP_MSU_ID
    };

    INIT_LOCAL_RUNTIME(LOCAL_RT_ID);

    ADD_ROUTE_TO_MSU(http_msu, write_msu, LOCAL_RT_ID, 13);

    struct read_state *read_state = malloc(sizeof(*read_state));

    sprintf(read_state->req, REQ_STR, "/", "localhost");
    read_state->req_len = strlen(read_state->req);

    struct msu_msg msg = {
       .data = read_state,
       .data_size = sizeof(*read_state)
    };

    add_provinance(&msg.hdr.provinance, &read_msu);

    int rtn = craft_http_response(&http_msu, &msg);

    ck_assert_int_eq(rtn, 0);
    ck_assert_int_eq(write_msu.queue.num_msgs, 1);

    FREE_MSU_ROUTES(http_msu);
} END_DEDOS_TEST

#define XSTR(F) #F
#define HTTP_INIT_STR(PORT) "127.0.0.1 " XSTR(PORT) " 5"

START_DEDOS_TEST(test_craft_http_response__valid_http__db_access__fail_invalid_addr) {

    init_statistics();

    log_info(HTTP_INIT_STR(DB_PORT));
    struct msu_init_data init_data = {
        HTTP_INIT_STR(DB_PORT)
    };

    sock_state.epoll_fd = init_epoll(-1);
    ck_assert_int_ne(sock_state.epoll_fd, -1);
    init_stat_item(MSU_STAT1, socket_msu.id);

    struct local_msu http_msu = {
        .type = &WEBSERVER_HTTP_MSU_TYPE,
        .id = HTTP_MSU_ID
    };
    int rtn = http_init(&http_msu, &init_data);
    ck_assert_int_eq(rtn, 0);

    INIT_LOCAL_RUNTIME(LOCAL_RT_ID);

    ADD_ROUTE_TO_MSU(http_msu, write_msu, LOCAL_RT_ID, 13);

    struct read_state *read_state = malloc(sizeof(*read_state));

    sprintf(read_state->req, REQ_STR, "/database", "localhost");
    read_state->req_len = strlen(read_state->req);

    struct msu_msg msg = {
       .data = read_state,
       .data_size = sizeof(*read_state)
    };

    add_provinance(&msg.hdr.provinance, &read_msu);

    rtn = craft_http_response(&http_msu, &msg);

    ck_assert_int_eq(rtn, 0);
    int n_fds = (int)get_last_stat(MSU_STAT1, socket_msu.id);
    ck_assert_int_eq(n_fds, 0);
    ck_assert_int_eq(write_msu.queue.num_msgs, 1);
    struct msu_msg *msg_out = dequeue_msu_msg(&write_msu.queue);

    struct response_state *resp = msg_out->data;
    ck_assert_int_eq(resp->conn.status, CON_ERROR);
    log_info("resp: %p", resp);
    free(resp);
    free(msg_out);
    FREE_MSU_ROUTES(http_msu);
    http_destroy(&http_msu);
} END_DEDOS_TEST


START_DEDOS_TEST(test_craft_http_response__valid_http__db_access) {

    init_statistics();

    struct msu_init_data init_data = {
        HTTP_INIT_STR(DB_PORT)
    };

    sock_state.epoll_fd = init_epoll(-1);
    ck_assert_int_ne(sock_state.epoll_fd, -1);
    init_stat_item(MSU_STAT1, socket_msu.id);

    int sock = init_test_listening_socket(DB_PORT);

    struct local_msu http_msu = {
        .type = &WEBSERVER_HTTP_MSU_TYPE,
        .id = HTTP_MSU_ID
    };
    int rtn = http_init(&http_msu, &init_data);
    ck_assert_int_eq(rtn, 0);

    INIT_LOCAL_RUNTIME(LOCAL_RT_ID);

    ADD_ROUTE_TO_MSU(http_msu, write_msu, LOCAL_RT_ID, 13);

    struct read_state *read_state = malloc(sizeof(*read_state));

    sprintf(read_state->req, REQ_STR, "/database", "localhost");
    read_state->req_len = strlen(read_state->req);

    struct msu_msg msg = {
       .data = read_state,
       .data_size = sizeof(*read_state)
    };

    add_provinance(&msg.hdr.provinance, &read_msu);

    rtn = craft_http_response(&http_msu, &msg);

    ck_assert_int_eq(rtn, 0);
    int n_fds = (int)get_last_stat(MSU_STAT1, socket_msu.id);
    ck_assert_int_eq(n_fds, 1);
    ck_assert_int_eq(write_msu.queue.num_msgs, 0);

    ck_assert_int_eq(msu_num_states(&http_msu), 1);

    int fd = accept(sock, NULL, 0);

    write(fd, "3", 2);

    close(fd);

    struct http_state *http_state = msu_get_state(&http_msu, &msg.hdr.key, NULL);
    struct socket_msg *smsg = malloc(sizeof(*smsg));
    smsg->fd = http_state->db.db_fd;

    msg.data = smsg;
    msg.data_size = sizeof(*smsg);
    rtn = craft_http_response(&http_msu, &msg);

    ck_assert_int_eq(rtn, 0);
    ck_assert_int_eq(write_msu.queue.num_msgs, 1);
    struct msu_msg *msg_out = dequeue_msu_msg(&write_msu.queue);
    struct response_state *resp = msg_out->data;
    ck_assert_int_ne(resp->conn.status, CON_ERROR);
    free(msg_out);
    free(resp);
    http_destroy(&http_msu);
    FREE_MSU_ROUTES(http_msu);
} END_DEDOS_TEST

DEDOS_START_TEST_LIST("http_msu")

DEDOS_ADD_TEST_FN(test_craft_http_response__return_to_read__incomplete)
DEDOS_ADD_TEST_FN(test_craft_http_response__send_error_to_write)
DEDOS_ADD_TEST_FN(test_craft_http_response__valid_http__no_db_no_regex)
DEDOS_ADD_TEST_FN(test_craft_http_response__valid_http__db_access__fail_invalid_addr)
DEDOS_ADD_TEST_FN(test_craft_http_response__valid_http__db_access)

DEDOS_END_TEST_LIST()


