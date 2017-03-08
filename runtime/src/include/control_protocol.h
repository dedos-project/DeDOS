#ifndef CONTROL_PROTOCOL_H_
#define CONTROL_PROTOCOL_H_

#include <stdint.h>
#include <stdlib.h>
#include <error.h>

/* Request or Response control message between runtime and master */
#define REQUEST_MESSAGE 1
#define RESPONSE_MESSAGE 2
#define ACTION_MESSAGE 3 //just an action to do no response required

/* msg_code field in dedos_control_msg
 * Various types of request/responses between runtime and master */
#define GET_INIT_CONFIG 10
#define SET_INIT_CONFIG 11
#define GET_MSU_LIST 12
#define SET_DEDOS_RUNTIMES_LIST 20 /* msg from master to runtime for connecting to other runtimes */
#define CREATE_PINNED_THREAD 21
#define DELETE_PINNED_THREAD 22
#define STATISTICS_UPDATE 30

/* actions for dedos_thread_msg */
#define CREATE_MSU 2001
#define DESTROY_MSU 2002
#define FORWARD_DATA 2003
#define MSU_ROUTE_ADD 2004
#define MSU_ROUTE_DEL 2005

#define FAIL_CREATE_MSU 4001
#define FAIL_DESTROY_MSU 4002

struct dedos_control_msg_manage_msu {
    int msu_id;
    unsigned int msu_type;
    unsigned int init_data_size;
    void *init_data;
};

struct dedos_control_msg {
    unsigned int msg_type; //request or response
    unsigned int msg_code;
    unsigned int header_len;
    unsigned int payload_len;
    void *payload;
};

/* actions for dedos_thread_msg */
#define CREATE_MSU 2001
#define DESTROY_MSU 2002
#define FORWARD_DATA 2003

struct create_msu_thread_msg_data
{
    uint16_t msu_type;
    uint32_t init_data_len;
    void* creation_init_data;
};

struct dedos_thread_msg
{
    struct dedos_thread_msg *next;
    uint16_t action;
    unsigned int action_data; // e.g for CREATE_MSU, could be id, for FORWARD dest runtime ip
    uint32_t buffer_len; //total len of data
    void *data; /* depending on the action and msu_id, creator and processor of
     the message should know how to handle this data */
};

struct dedos_thread_msg* dedos_thread_msg_alloc();
void dedos_thread_msg_free(struct dedos_thread_msg* msg);

#endif /* CONTROL_PROTOCOL_H_ */
