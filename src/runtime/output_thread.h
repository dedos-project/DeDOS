#ifndef OUTPUT_THREAD_H_
#define OUTPUT_THREAD_H_

#include "dedos_threads.h"
#include "thread_message.h"

#define OUTPUT_THREAD_ID -1

int enqueue_for_output(struct thread_msg *thread_msg);

void stop_output_monitor();
void join_output_thread();

struct dedos_thread *start_output_monitor_thread(void);

#endif
