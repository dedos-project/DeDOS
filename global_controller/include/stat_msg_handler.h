#ifndef STAT_MSG_HANDLER_H
#define STAT_MSG_HANDLER_H

#include <stdint.h>
#include <pthread.h>

#include "logging.h"
#include "dedos_statistics.h"
#include "control_protocol.h"

int process_stats_msg(struct stats_control_payload *stats, int runtime_sock);

#endif
