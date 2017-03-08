/*
 * control_operations.h
 *
 * Operations related to creation, deletion of MSU's
 * and other control stuff
 */
#ifndef CONTROL_OPERATIONS_H_
#define CONTROL_OPERATIONS_H_

#include "runtime.h"
#include "communication.h"
#include "control_protocol.h"

int create_msu_request(struct dedos_thread *d_thread, struct dedos_thread_msg* thread_msg);
int delete_msu_request(struct dedos_thread *d_thread, struct dedos_thread_msg* thread_msg);
int enqueue_msu_request(struct dedos_thread *d_thread, struct dedos_thread_msg* thread_msg);

void process_control_updates(void); //called by each thread to process updates from its queue

#endif /* CONTROL_OPERATIONS_H_ */
