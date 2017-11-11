#ifndef WEBSERVER_H_
#define WEBSERVER_H_
#include <sys/epoll.h>

#define WS_COMPLETE         ((int)0x01)
#define WS_INCOMPLETE_READ  ((int)0x02)
#define WS_INCOMPLETE_WRITE ((int)0x04)
#define WS_ERROR            ((int)0x08)

enum webserver_status {
    NIL,
    CON_ERROR,
    NO_CONNECTION,
    CON_ACCEPTED,
    CON_SSL_CONNECTING,
    CON_READING,
    CON_DB_REQUEST,
    CON_WRITING,
};

#define RTN_TO_EVT(rtn__) (rtn__ & WS_INCOMPLETE_READ ? EPOLLIN : 0) | \
                          (rtn__ & WS_INCOMPLETE_WRITE ? EPOLLOUT : 0)

#endif
