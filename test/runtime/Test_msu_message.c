#undef DEDOS_PROFILER
#include "dedos_testing.h"

// Include file under test
#include "msu_message.c"

START_DEDOS_TEST(test_enqueue_and_dequeue_msu_msg) {

    struct msg_queue q;
    init_msg_queue(&q, NULL);

    struct msu_msg msg1 = {};
    struct msu_msg msg2 = {};
    struct msu_msg msg3 = {};

    ck_assert_int_eq(enqueue_msu_msg(&q, &msg1), 0);
    ck_assert_int_eq(enqueue_msu_msg(&q, &msg2), 0);
    ck_assert_int_eq(enqueue_msu_msg(&q, &msg3), 0);

    struct msu_msg *out;
    out = dequeue_msu_msg(&q);
    ck_assert_ptr_eq(out, &msg1);
    out = dequeue_msu_msg(&q);
    ck_assert_ptr_eq(out, &msg2);
    out = dequeue_msu_msg(&q);
    ck_assert_ptr_eq(out, &msg3);
    out = dequeue_msu_msg(&q);
    ck_assert_ptr_eq(out, NULL);
} END_DEDOS_TEST

struct msu_type default_type = {};
struct local_msu default_msu= {
    .type = &default_type
};
START_DEDOS_TEST(test_serialize_and_read_msu_msg__default) {

    int data = 12345;

    struct msu_msg msg = {
        .hdr = {
            .key = {{1234}}
        },
        .data_size = sizeof(data),
        .data = &data
    };



    int fds[2];
    fds[0] = init_sock_pair(&fds[1]);

    size_t output_size;
    void *output = serialize_msu_msg(&msg, &default_type, &output_size);
    ck_assert_int_eq(output_size, sizeof(msg.hdr) + sizeof(data));

    write(fds[1], output, output_size);
    struct msu_msg *out = read_msu_msg(&default_msu, fds[0], output_size);

    ck_assert_int_eq(out->hdr.key.key.k1, msg.hdr.key.key.k1);

    ck_assert_int_eq(out->data_size, msg.data_size);
    ck_assert_int_eq(*(int*)out->data, data);

    free(out->data);
    free(out);
    free(output);
} END_DEDOS_TEST

struct layer_2 {
    int value;
};

struct layer_1 {
    int value_1;
    struct layer_2 *l2;
};

ssize_t custom_serialize(struct msu_type *type, struct msu_msg *input, void **output) {
    struct layer_1 *l1 = input->data;

    *output = malloc(sizeof(*l1) + sizeof(*l1->l2));

    memcpy(*output, l1, sizeof(*l1));
    memcpy(*output + sizeof(*l1), l1->l2, sizeof(*l1->l2));

    return sizeof(*l1) + sizeof(*l1->l2);
}

void *custom_deserialize(struct local_msu *msu, size_t input_size, void *input, size_t *size_out) {
    struct layer_1 *l1 = malloc(sizeof(*l1));

    memcpy(l1, input, sizeof(*l1));
    l1->l2 = malloc(sizeof(*l1->l2));

    memcpy(l1->l2, input + sizeof(*l1), sizeof(*l1->l2));
    *size_out = sizeof(*l1);
    log(TEST, "Set size_out to %d", (int)*size_out);
    return l1;
}

struct msu_type custom_type = {
    .id = 134,
    .name = "CUSTOM",
    .serialize = custom_serialize,
    .deserialize = custom_deserialize
};

struct local_msu custom_msu = {
    .id = 42,
    .type = &custom_type
};

START_DEDOS_TEST(test_serialize_and_read_msu_msg__custom) {
    struct msu_msg_hdr hdr = {
        .key = {
            .key = {1234},
        }
    };

    struct layer_2 l2 = {.value = 12345};

    struct layer_1 data = {
        .value_1 = 4361,
        .l2 = &l2
    };

    struct msu_msg msg = {
        .hdr = { .key = {{1234}} },
        .data_size = sizeof(data),
        .data = &data
    };

    size_t sz;
    void *serialized = serialize_msu_msg(&msg, &custom_type, &sz);

    int fds[2];
    fds[0] = init_sock_pair(&fds[1]);

    write(fds[1], serialized, sz);

    struct msu_msg *out = read_msu_msg(&custom_msu, fds[0], sz);

    ck_assert_int_eq(out->hdr.key.key.k1, hdr.key.key.k1);
    ck_assert_int_eq(out->data_size, msg.data_size);
    struct layer_1 *data_out = out->data;
    ck_assert_int_eq(data_out->value_1, data.value_1);
    ck_assert_int_eq(data_out->l2->value, data.l2->value);

    free(serialized);
    free(data_out->l2);
    free(data_out);
    free(out);

} END_DEDOS_TEST

START_DEDOS_TEST(test_add_provinance) {
    struct msu_msg_hdr hdr = {
        .key = {
            .key = {1234}
        },
        .provinance = {}
    };

    struct dfg_runtime rt = {
        .ip = 1
    };
    set_local_runtime(&rt);

    ck_assert_int_eq(add_provinance(&hdr.provinance, &custom_msu),0);
    ck_assert_int_eq(hdr.provinance.path_len, 1);
    ck_assert_int_eq(hdr.provinance.path[0].type_id, custom_type.id);
    ck_assert_int_eq(hdr.provinance.path[0].msu_id, custom_msu.id);
    ck_assert_int_eq(hdr.provinance.path[0].runtime_id, rt.id);

    ck_assert_int_eq(add_provinance(&hdr.provinance, &custom_msu),0);
    ck_assert_int_eq(hdr.provinance.path_len, 2);
    ck_assert_int_eq(hdr.provinance.path[1].type_id, custom_type.id);
    ck_assert_int_eq(hdr.provinance.path[1].runtime_id, rt.id);
} END_DEDOS_TEST

DEDOS_START_TEST_LIST("MSU_message")

DEDOS_ADD_TEST_FN(test_enqueue_and_dequeue_msu_msg)
DEDOS_ADD_TEST_FN(test_serialize_and_read_msu_msg__default)
DEDOS_ADD_TEST_FN(test_serialize_and_read_msu_msg__custom)
DEDOS_ADD_TEST_FN(test_add_provinance)

DEDOS_END_TEST_LIST()
