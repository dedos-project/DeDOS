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
#ifndef API_H_
#define APIP_H_

#include "dfg.h"

int add_msu(unsigned int msu_id, unsigned int type_id,
            char *init_data_c, char *msu_mode, char *vertex_type,
            unsigned int thread_id, unsigned int runtime_id);

int remove_msu(unsigned int id);

int create_route(unsigned int route_id, unsigned int type_id, unsigned int runtime_id);
int delete_route(unsigned int route_id);
int add_route_to_msu(unsigned int msu_id, unsigned int route_id);
int del_route_from_msu(unsigned int msu_id, unsigned int route_id);
int add_endpoint(unsigned int msu_id, uint32_t key, unsigned int route_id);
int del_endpoint(unsigned int msu_id, unsigned int route_id);
int mod_endpoint(unsigned int msu_id, uint32_t key, unsigned int route_id);
int create_worker_thread(unsigned int thread_id, unsigned int runtime_id, char *mode);

#endif
