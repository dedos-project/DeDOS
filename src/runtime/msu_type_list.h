// INCLUDE INDIVIDUAL MSU TYPE HEADERS
#include "test_msu.h"
#include "socket_msu.h"
#include "webserver/read_msu.h"
#include "webserver/http_msu.h"
#include "webserver/regex_msu.h"
#include "webserver/regex_routing_msu.h"
#include "webserver/write_msu.h"

#include "ndlog/ndlog_msu_R1_eca.h"
#include "ndlog/ndlog_recv_msu.h"
#include "ndlog/ndlog_routing_msu.h"

#include "pico_tcp/msu_pico_tcp.h"
#include "pico_tcp/msu_tcp_handshake.h"

#ifndef MSU_TYPE_LIST
#define MSU_TYPE_LIST  \
    { \
        &TEST_MSU_TYPE, \
        &SOCKET_MSU_TYPE, \
        &WEBSERVER_READ_MSU_TYPE, \
        &WEBSERVER_HTTP_MSU_TYPE, \
        &WEBSERVER_REGEX_MSU_TYPE, \
        &WEBSERVER_REGEX_ROUTING_MSU_TYPE, \
        &WEBSERVER_WRITE_MSU_TYPE, \
        \
        &NDLOG_MSU_TYPE, \
        &NDLOG_RECV_MSU_TYPE, \
        &NDLOG_ROUTING_MSU_TYPE, \
        \
        &PICO_TCP_MSU_TYPE, \
        &TCP_HANDSHAKE_MSU_TYPE, \
    /* LIST MSU TYPES HERE!! */ \
    }
#endif
