#ifndef DFG_INTERPRETER_H
#define DFG_INTERPRETER_H
#include "dfg.h"

struct dfg_runtime_endpoint *get_local_runtime(struct dfg_config *dfg, int runtime_id);
int implement_dfg(struct dfg_config *dfg, int runtime_id);

#endif
