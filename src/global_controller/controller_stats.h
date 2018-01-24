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
#ifndef MSU_STATS_H_
#define MSU_STATS_H_

#include "timeseries.h"
#include "stats.h"
#include "dfg.h"
#include "unused_def.h"

struct stat_item {
    unsigned int id;
    struct timed_rrdb stats;
};

#define MAX_STAT_ID 4192


struct timed_rrdb *get_runtime_stat(enum stat_id id, unsigned int runtime_id);
struct timed_rrdb *get_msu_stat(enum stat_id id, unsigned int msu_id);
struct timed_rrdb *get_thread_stat(enum stat_id id, unsigned int thread_id, unsigned int runtime_id);

int unregister_msu_stats(unsigned int msu_id);
int register_msu_stats(unsigned int msu_id, int msu_type_id, int thread_id, int runtime_id);
int unregister_thread_stats(unsigned int thread_id, unsigned int runtime_id);
int register_thread_stats(unsigned int thread_id, unsigned int runtime_id);
int init_statistics();

void show_stats(struct dfg_msu *msu);

#endif
