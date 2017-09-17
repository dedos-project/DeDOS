#ifndef STAT_MSG_HANDLER_H
#define STAT_MSG_HANDLER_H

int init_stats_msg_handler();

int handle_serialized_stats_buffer(void *buffer, size_t buffer_len);

//int process_stats_msg(struct stats_control_payload *stats, int runtime_sock);

#endif
