#ifndef DEDOS_MSU_MSG_TYPE_H_
#define DEDOS_MSU_MSG_TYPE_H_

/*
 * Message type for each message between MSU's on the control socket
 * should be defined here
 */

#define MSU_PROTO_DUMMY 99
#define MSU_PROTO_TCP_HANDSHAKE 100
#define MSU_PROTO_TCP_HANDSHAKE_RESPONSE 101
#define MSU_PROTO_TCP_CONN_RESTORE 102
#define MSU_PROTO_TCP_ROUTE_HS_REQUEST 103

#define MSU_PROTO_PICO_TCP_STACK 500
#define MSU_PROTO_PICO_TCP_APP_ECHO 501

#define MSU_PROTO_SSL_REQUEST 701

#define MSU_PROTO_REGEX 801

#endif /* DEDOS_MSU_MSG_TYPE_H_ */
