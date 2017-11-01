#ifndef RUNTIME_DFG_H_
#define RUNTIME_DFG_H_
#include <netinet/ip.h>
#include "dfg.h"

void set_local_runtime(struct dfg_runtime *rt);
int init_runtime_dfg(char *filename, int runtime_id);
int controller_address(struct sockaddr_in *addr);
int local_runtime_id();
int local_runtime_port();
uint32_t local_runtime_ip();
struct dedos_dfg *get_dfg();

#endif


