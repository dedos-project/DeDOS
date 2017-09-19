// INCLUDE INDIVIDUAL MSU TYPE HEADERS
#include "test_msu.h"
#include "socket_msu.h"
#include "webserver/read_msu.h"
#include "webserver/http_msu.h"
#include "webserver/regex_msu.h"
#include "webserver/regex_routing_msu.h"
#include "webserver/write_msu.h"

#ifndef MSU_TYPE_LIST
#define MSU_TYPE_LIST  \
    { \
        &TEST_MSU_TYPE, \
        &SOCKET_MSU_TYPE, \
        &WEBSERVER_READ_MSU_TYPE, \
        &WEBSERVER_HTTP_MSU_TYPE, \
        &WEBSERVER_REGEX_MSU_TYPE, \
        &WEBSERVER_REGEX_ROUTING_MSU_TYPE, \
        &WEBSERVER_WRITE_MSU_TYPE \
    /* LIST MSU TYPES HERE!! */ \
    }
#endif
