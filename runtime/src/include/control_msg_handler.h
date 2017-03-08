#ifndef CONTROL_MSG_HANDLER_H_
#define CONTROL_MSG_HANDLER_H_
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>
#include <mcheck.h>
#include <pthread.h>
#include <stdint.h>
#include "control_msg_handler.h"
#include "logging.h"

void parse_cmd_action(char *cmd);

#endif /* CONTROL_MSG_HANDLER_H_ */
