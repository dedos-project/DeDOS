#include "dfg.h"

void print_dfg(void);
void dfg_to_file(struct dfg_config *dfg, int n_statistics, char *filename);
char *dfg_to_json(struct dfg_config *dfg, int n_statistics);
