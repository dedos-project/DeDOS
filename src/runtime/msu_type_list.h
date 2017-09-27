#ifndef MSU_TYPE_LIST_H_
#define MSU_TYPE_LIST_H_
// INCLUDE INDIVIDUAL MSU TYPE HEADERS
#include "test_msu.h"
#include "socket_msu.h"

// These ifdef guards below allow you to optionally compile different groups of MSUS
// See the Makefile for the list of compilable MSU groups

#ifdef COMPILE_WEBSERVER_MSUS
#include "webserver/read_msu.h"
#include "webserver/http_msu.h"
#include "webserver/regex_msu.h"
#include "webserver/regex_routing_msu.h"
#include "webserver/write_msu.h"
#define WEBSERVER_MSUS \
        &WEBSERVER_READ_MSU_TYPE, \
        &WEBSERVER_HTTP_MSU_TYPE, \
        &WEBSERVER_REGEX_MSU_TYPE, \
        &WEBSERVER_REGEX_ROUTING_MSU_TYPE, \
        &WEBSERVER_WRITE_MSU_TYPE,
#else
#define WEBSERVER_MSUS
#endif

#ifdef COMPILE_NDLOG_MSUS
#include "ndlog/ndlog_msu_R1_eca.h"
#include "ndlog/ndlog_recv_msu.h"
#include "ndlog/ndlog_routing_msu.h"
#define NDLOG_MSUS \
        &NDLOG_MSU_TYPE, \
        &NDLOG_RECV_MSU_TYPE, \
        &NDLOG_ROUTING_MSU_TYPE,
#else
#define NDLOG_MSUS
#endif

#ifdef COMPILE_PICO_TCP_MSUS
#include "pico_tcp/msu_pico_tcp.h"
#include "pico_tcp/msu_tcp_handshake.h"
#define PICO_TCP_MSUS \
        &PICO_TCP_MSU_TYPE, \
        &TCP_HANDSHAKE_MSU_TYPE,
#else
#define PICO_TCP_MSUS
#endif

#ifndef MSU_TYPE_LIST
#define MSU_TYPE_LIST  \
    { \
        &TEST_MSU_TYPE, \
        &SOCKET_MSU_TYPE, \
        WEBSERVER_MSUS \
        NDLOG_MSUS \
        PICO_TCP_MSUS \
    }
#endif

#endif
