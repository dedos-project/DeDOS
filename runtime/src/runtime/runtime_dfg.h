#ifndef RUNTIME_DFG_H_
#define RUNTIME_DFG_H_
#include <netinet/ip.h>

int init_runtime_dfg(char *filename, int runtime_id);
int controller_address(struct sockaddr_in *addr);
int local_runtime_id();
int local_runtime_port();
struct dedos_dfg *get_dfg();

#endif


