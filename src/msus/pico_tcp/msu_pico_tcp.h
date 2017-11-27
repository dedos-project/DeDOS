#ifndef MSU_PICO_TCP_H
#define MSU_PICO_TCP_H

#include "msu_type.h"
#include "pico_frame.h"

#define MSU_PROTO_TCP_HANDSHAKE 100
#define MSU_PROTO_TCP_HANDSHAKE_RESPONSE 101
#define MSU_PROTO_TCP_CONN_RESTORE 102
struct pico_tcp_msg {
    unsigned int proto_msg_type;
    void * payload;
    size_t payload_len;
};

#define DEDOS_TCP_DATA_MSU_TYPE_ID 100

struct msu_type PICO_TCP_MSU_TYPE;
int32_t pico_tcp_generate_id(struct local_msu *self, struct pico_frame *f);

#endif
