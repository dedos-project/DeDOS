#ifndef _SCHEDULING_CUT_H
#define _SCHEDULING_CUT_H

struct cut {
    uint16_t num_cores;
    uint64_t dram;
    uint64_t io_network_bw;
    uint64_t egress_bw;
    uint64_t ingress_bw;
    int num_msu;
    int *msu_ids[MAX_MSU];
};

#endif
