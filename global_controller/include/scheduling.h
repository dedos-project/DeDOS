#ifndef SCHEDULING_H_
#define SCHEDULING_H_

#include "dfg.h"
#include "scheduling_cut.h"


struct to_schedule {
    int *msu_ids;
    int num_msu;
};

int (*policy)(struct to_schedule *ts, struct dfg_config *dfg);
int find_placement(int *runtime_sock, int *thread_id);
int allocate(struct to_schedule *ts);
int init_scheduler(const char *policy);
struct runtime_thread *find_thread(struct dfg_vertex *msu, struct dfg_runtime_endpoint *runtime);
uint64_t compute_out_of_cut_bw(struct dfg_vertex *msu, struct cut *c, const char *direction);
int greedy_policy(struct to_schedule *ts, struct dfg_config *dfg);

int set_edges(struct to_schedule *ts, struct dfg_config *dfg);
int set_msu_deadlines(struct to_schedule *ts, struct dfg_config *dfg);

#endif /* !SCHEDULING_H_ */
