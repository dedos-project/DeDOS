#ifndef CONTROLLER_DFG_H__
#define CONTROLLER_DFG_H__
#include "dfg.h"

int init_controller_dfg(char *filename);
int local_listen_port();
int generate_msu_id();
int generate_route_id();
uint32_t generate_endpoint_key(struct dfg_route *route);
struct dedos_dfg *get_dfg(void);

#endif
