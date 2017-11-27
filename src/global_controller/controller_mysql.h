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
#ifndef CONTROLLER_MYSQL_H
#define CONTROLLER_MYSQL_H

#include "dfg.h"
#include "stats.h"


int db_init(int clear);
int db_terminate();

int db_check_and_register(const char *check_query, const char *insert_query,
                          const char *element, int thread_id);
int db_register_runtime(int runtime_id);
int db_register_thread_stats(int thread_id, int runtime_id);
int db_register_msu_stats(int msu_id, int msu_type_id, int thread_id, int runtime_id);
int db_register_msu(int msu_id, int msu_type_id, int thread_id, int runtime_id);

int db_insert_sample(struct timed_stat *input, struct stat_sample_hdr *input_hdr, int runtime_id);

#endif
