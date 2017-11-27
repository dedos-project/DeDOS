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
#ifndef RUNTIME_MESSAGES_H_
#define RUNTIME_MESSAGES_H_

#include "dfg.h"

int send_create_msu_msg(struct dfg_msu *msu);

int send_delete_msu_msg(struct dfg_msu *msu);

int send_create_route_msg(struct dfg_route *route);
int send_delete_route_msg(struct dfg_route *route);
int send_add_route_to_msu_msg(struct dfg_route *route, struct dfg_msu *msu);
int send_add_endpoint_msg(struct dfg_route *route, struct dfg_route_endpoint *endpoint);
int send_del_endpoint_msg(struct dfg_route *route, struct dfg_route_endpoint *endpoint);
int send_mod_endpoint_msg(struct dfg_route *route, struct dfg_route_endpoint *endpoint);
int send_create_thread_msg(struct dfg_thread *thread, struct dfg_runtime *rt);

int send_report_msus_msg(struct dfg_runtime *rt);

#endif

