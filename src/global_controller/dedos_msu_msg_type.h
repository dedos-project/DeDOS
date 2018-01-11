/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

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
#endif /* DEDOS_MSU_MSG_TYPE_H_ */
