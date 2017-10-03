#ifndef MAIN_THREAD_H_
#define MAIN_THREAD_H_

#include "dedos_threads.h"
#include "thread_message.h"

#define MAIN_THREAD_ID 0

int enqueue_to_main_thread(struct thread_msg *thread_msg);

void stop_main_thread();
void join_main_thread();
struct dedos_thread *start_main_thread(void);

#endif
