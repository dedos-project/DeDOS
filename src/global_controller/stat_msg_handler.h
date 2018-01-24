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
#ifndef STAT_MSG_HANDLER_H
#define STAT_MSG_HANDLER_H
#include "stats.h"
#include <stdlib.h>

int init_stats_msg_handler();

int handle_serialized_stats_buffer(int runtime_id, void *buffer, size_t buffer_len);

//int process_stats_msg(struct stats_control_payload *stats, int runtime_sock);

int set_rt_stat_limit(int runtime_id, struct stat_limit *lim);

#endif
