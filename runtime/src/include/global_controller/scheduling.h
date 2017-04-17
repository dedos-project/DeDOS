#ifndef SCHEDULING_H_
#define SCHEDULING_H_


struct cut {
    uint16_t num_cores;
    uint64_t dram;
    uint64_t bissection_bw;
    uint64_t egress_bw;
    uint64_t ingress_bw;
    int num_msu;
};

#include "dfg.h"

struct to_schedule {
    int *msu_ids;
    int num_msu;
};

int (*policy)(struct to_schedule *ts, struct dfg_config *dfg);
int find_placement(int *runtime_sock, int *thread_id);
int allocate(struct to_schedule *ts);
int init_scheduler(const char *policy);
//struct runtime_thread *find_thread(struct dfg_vertex *msu, struct dfg_runtime_endpoint *runtime);
uint64_t compute_out_of_cut_bw(struct dfg_vertex *msu, struct cut *c, const char *direction);
int greedy_policy(struct to_schedule *ts, struct dfg_config *dfg);

int set_edges(struct to_schedule *ts, struct dfg_config *dfg);
int set_msu_deadlines(struct to_schedule *ts, struct dfg_config *dfg);

#endif /* !SCHEDULING_H_ */
