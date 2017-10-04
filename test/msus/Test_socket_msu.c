#include "dedos_testing.h"

#include "socket_msu.h"
#include "msu_type.h"


int receive_fn(struct local_msu *self, struct msu_msg *msg) {
    return 0;
}

#define test_type_id 42

struct msu_type test_type = {
    .name = "Test_type",
    .id = test_type_id,
    .receive = receive_fn
};

#define MSU_TYPE_LIST \
{ \
    &test_type, \
    &SOCKET_MSU_TYPE \
}

#include "socket_msu.c"
#include "msu_type.c"
#include "runtime_dfg.c"

#define test_runtime_id 1

struct dfg_runtime rt = {
    .id = test_runtime_id
};

#define test_msu_id 422

struct dedos_thread d_thread = {};
struct worker_thread thread = {
    .thread = &d_thread
};

struct local_msu sock_msu = {
    .type = &SOCKET_MSU_TYPE,
    .id = 66,
    .thread = &thread
};

static struct sock_msu_state state = {
    .self = &sock_msu,
    .default_target = &test_type
};

int route_id = 2;

struct local_msu *create_target() {
    init_msu_type_id(test_type_id);
    struct local_msu *msu = init_msu(test_msu_id, &test_type,  &thread, NULL);
    return msu;
}

void route_to_target(struct local_msu *sock_msu){
    ck_assert_int_eq(init_route(route_id, test_type_id), 0);
    add_route_to_set(&sock_msu->routes, route_id);
    struct msu_endpoint ep;
    init_msu_endpoint(test_msu_id, test_runtime_id, &ep);
    ck_assert_int_eq(add_route_endpoint(route_id, ep, 1), 0);
}


START_DEDOS_TEST(test_process_connection__new) {

    struct local_msu *msu = create_target();
    route_to_target(&sock_msu);

    int listen_port = 1234;
    init_listening_socket(listen_port);
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr.s_addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listen_port);
    int rtn = connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr));
    if (rtn < 0) {
        log_perror("Error connecting");
    }

    rtn = process_connection(sock_fd, &state);


    ck_assert_int_eq(rtn, 0);
    ck_assert_int_eq(msu->queue.num_msgs, 1);
} END_DEDOS_TEST

START_DEDOS_TEST(test_process_connection__existing) {
    create_target();
    route_to_target(&sock_msu);

    struct msu_msg_hdr hdr = {};

    int fd = 10;
    state.destinations[fd] = &sock_msu;
    state.hdr_mask[fd] = hdr;

    process_connection(fd, &state);

    ck_assert_int_eq(sock_msu.queue.num_msgs, 1);

} END_DEDOS_TEST

DEDOS_START_TEST_LIST("test_socket_msu")
    LOCAL_RUNTIME = &rt;

DEDOS_ADD_TEST_FN(test_process_connection__new)
DEDOS_ADD_TEST_FN(test_process_connection__existing)

DEDOS_END_TEST_LIST()
