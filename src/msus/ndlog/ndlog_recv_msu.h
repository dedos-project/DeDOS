#ifndef NDLOG_RECV_MSU_H_
#define NDLOG_RECV_MSU_H_

#include "msu_type.h"
#include <inttypes.h>
#include <stdio.h>

#define NDLOG_RECV_MSU_TYPE_ID 912
extern struct msu_type NDLOG_RECV_MSU_TYPE;

typedef struct recv_state {
  uint32_t count;
  uint32_t received;
  int first_packet;
  struct timespec ts;
  struct timespec te;
  //bool has_showed;
  int experiment_num;
} recv_state;

extern recv_state* recv_msu_global_state;
extern int checkPkt[9];
extern int line_pos[9];
//extern vector<string> file_vec;

// struct generic_msu* ndlog_recv_msu_init(unsigned int id, void *initial_state_info);
// void ndlog_recv_msu_destroy(struct generic_msu *msu);
// int ndlog_recv_msu_process_queue_item(struct generic_msu *msu, struct generic_msu_queue_item *queue_item);
// int ndlog_recv_msu_process_control_q_item(struct generic_msu *msu, struct generic_msu_queue_item* control_q_item);

// struct generic_msu* ndlog_recv_msu1;
// struct generic_msu* ndlog_recv_msu2;
// static int ndlog_recv_msu1_inited = 0;
// static int ndlog_recv_msu1_taken = 0;

void print_recv_msu_global_state();

#endif /* NDLOG_RECV_MSU_H_ */
