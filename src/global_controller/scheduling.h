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
#ifndef SCHEDULING_H_
#define SCHEDULING_H_

#include "dfg.h"
#include "scheduling_cut.h"

struct to_schedule {
    int *msu_ids;
    int num_msu;
};

void prepare_clone(struct dfg_msu *msu);
struct dfg_msu *clone_msu(int msu_id);
int unclone_msu(int msu_id);
int fix_all_route_ranges(struct dedos_dfg *dfg);
int schedule_msu(struct dfg_msu *msu, struct dfg_runtime *rt, struct dfg_msu **new_msus);
int place_on_runtime(struct dfg_runtime *rt, struct dfg_msu *msu);

/* Related to previous allocation draft */
int (*policy)(struct to_schedule *ts, struct dedos_dfg *dfg);
int allocate(struct to_schedule *ts);
int init_scheduler(const char *policy);
struct dfg_thread *find_thread(struct dfg_msu *msu, struct dfg_runtime *runtime);
uint64_t compute_out_of_cut_bw(struct dfg_msu *msu, struct cut *c, const char *direction);
int greedy_policy(struct to_schedule *ts, struct dedos_dfg *dfg);

int set_edges(struct to_schedule *ts, struct dedos_dfg *dfg);
int set_msu_deadlines(struct to_schedule *ts, struct dedos_dfg *dfg);

#endif /* !SCHEDULING_H_ */
