#ifndef STAT_MSG_HANDLER_H
#define STAT_MSG_HANDLER_H

#include <stdint.h>
#include <pthread.h>

#include "logging.h"
#include "control_protocol.h"

void process_stats_msg(struct msu_stats_data *stats_data, int runtime_sock, int stats_items);

#endif
