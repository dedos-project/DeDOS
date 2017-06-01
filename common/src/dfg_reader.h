#pragma once
#include "dfg.h"

struct dfg_config *get_dfg();

struct dfg_config *parse_dfg_json(const char *filename);

int load_dfg_from_file(const char *init_filename);
