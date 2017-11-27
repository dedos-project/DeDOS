/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
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
