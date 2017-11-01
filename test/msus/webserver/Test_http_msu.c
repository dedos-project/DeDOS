#include "dedos_testing.h"
#include "routing_test_utils.h"

// Include file under test
#include "webserver/http_msu.c"

#define DB_PORT 9999
#define DB_REQUEST "/database"

#define HTTP_MSU_ID 2

struct local_msu http_msu = {
    .type = &WEBSERVER_HTTP_MSU_TYPE,
    .id = HTTP_MSU_ID,
};

#define READ_ROUTE_ID 123

#define READ_MSU_ID 3
struct local_msu read_msu = {
    .type = &WEBSERVER_READ_MSU_TYPE,
    .id = READ_MSU_ID
};

#define LOCAL_RT_ID 1

START_DEDOS_TEST(test_craft_http_response__read_only) {
    INIT_LOCAL_RUNTIME(LOCAL_RT_ID);

    ADD_ROUTE_TO_MSU(http_msu, read_msu, LOCAL_RT_ID, 12);

    int fd_in;
    int fd_out = init_sock_pair(&fd_in);

    struct read_state read_state = {
        .conn = {
            .fd = fd_out
        },
        .req = "",
        .req_len = 0
    };

    struct msu_msg msg = {
        .data = &read_state,
        .data_size = sizeof(read_state)
    };

    add_provinance(&msg.hdr.provinance, &read_msu);

    int rtn = craft_http_response(&http_msu, &msg);

    ck_assert_int_eq(rtn, 0);
    close(fd_in);
    close(fd_out);


} END_DEDOS_TEST


START_DEDOS_TEST(test_craft_http_response__db) {
    int db_sock = init_test_listening_socket(DB_PORT);
    init_db("127.0.0.1", DB_PORT, 100);

    struct http_state http_state = {};

    struct local_msu self = {};

    struct msu_msg msg = {};

    handle_db(&http_state, &self, &msg);
} END_DEDOS_TEST

DEDOS_START_TEST_LIST("http_msu")

DEDOS_ADD_TEST_FN(test_craft_http_response__read_only)

DEDOS_END_TEST_LIST()


