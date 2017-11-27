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
#ifndef CONTROLLER_DFG_H__
#define CONTROLLER_DFG_H__
#include "dfg.h"

int init_controller_dfg(char *filename);
int local_listen_port();
int generate_msu_id();
int generate_route_id();
uint32_t generate_endpoint_key(struct dfg_route *route);
struct dedos_dfg *get_dfg(void);

int init_dfg_lock();
int lock_dfg();
int unlock_dfg();


#endif
