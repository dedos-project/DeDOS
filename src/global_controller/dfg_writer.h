#ifndef DFG_WRITER_H_
#define DFG_WRITER_H_
#include "dfg.h"

char *dfg_to_json(struct dedos_dfg *dfg, int n_stats);
void dfg_to_file(char *filename);
int dfg_to_fd(int fd);

#endif
