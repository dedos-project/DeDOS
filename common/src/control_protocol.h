#ifndef CONTROL_PROTOCOL_H_
#define CONTROL_PROTOCOL_H_

#include <stdint.h>
#include <stdlib.h>
#include <error.h>

#define MAX_RCV_BUFLEN 16384

/* Request or Response control message between runtime and master */
enum dedos_msg_type {
    REQUEST_MESSAGE = 1,
    RESPONSE_MESSAGE = 2,
    ACTION_MESSAGE = 3 // No response required
};

/* msg_code field in dedos_control_msg
 * Various types of request/responses between runtime and master */
enum dedos_action_type {
    GET_INIT_CONFIG = 10,
    SET_INIT_CONFIG = 11,
    GET_MSU_LIST = 12,
    RUNTIME_ENDPOINT_ADD = 13,
    SET_DEDOS_RUNTIMES_LIST = 20, // msg from master to runtime for connecting to other runtimes
    CREATE_PINNED_THREAD = 21,
    DELETE_PINNED_THREAD = 22,
    STATISTICS_UPDATE = 30,
    /* actions for dedos_thread_msg */
    CREATE_MSU   = 2001,
    DESTROY_MSU  = 2002,
    FORWARD_DATA = 2003,
    ADD_ROUTE_TO_MSU   = 3001,
    DEL_ROUTE_FROM_MSU = 3002,
    ROUTE_ADD_ENDPOINT = 3011,
    ROUTE_DEL_ENDPOINT = 3012,
    ROUTE_MOD_ENDPOINT = 3013
};

enum locality {
    MSU_IS_LOCAL = 1,
    MSU_IS_REMOTE = 2
};

#define FAIL_CREATE_MSU 4001
#define FAIL_DESTROY_MSU 4002

//struct dedos_control_msg_manage_msu {

struct manage_route_control_payload {
    int msu_id;
    int msu_type_id;
    int route_id;
    uint32_t key_range;
    enum locality locality;
    uint32_t ipv4;
};

struct manage_msu_control_payload {
    int msu_id;
    unsigned int msu_type;

    int route_id; // Optional -- only set if action pertains to a route
    unsigned int init_data_size;
    void *init_data;
};

struct stats_control_payload {
    int n_samples;
    struct stat_sample *samples;
};

struct dedos_control_msg {
    enum dedos_msg_type msg_type; //request or response
    enum dedos_action_type msg_code;
    unsigned int header_len;
    unsigned int payload_len;
    void *payload;
};

struct create_msu_thread_data {
    uint16_t msu_type;
    int msu_id;
    uint32_t init_data_len;
    void* init_data;
};

struct msu_action_thread_data {
    int msu_id;
    enum dedos_action_type action;
    int route_id; // Optional -- only set if action pertains to a route
};

struct dedos_thread_msg
{
    struct dedos_thread_msg *next;
    enum dedos_action_type action;
    unsigned int action_data; // e.g for CREATE_MSU, could be id, for FORWARD dest runtime ip
    uint32_t buffer_len; //total len of data
    void *data; /* depending on the action and msu_id, creator and processor of
     the message should know how to handle this data */
};

int send_control_msg(int sock, struct dedos_control_msg *control_msg);
struct dedos_thread_msg* dedos_thread_msg_alloc();
void dedos_thread_msg_free(struct dedos_thread_msg* msg);

#endif /* CONTROL_PROTOCOL_H_ */
