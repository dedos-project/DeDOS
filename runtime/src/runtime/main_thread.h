#ifndef MAIN_THREAD_H_
#define MAIN_THREAD_H_

#include "dedos_threads.h"

#define MAIN_THREAD_ID 0

struct dedos_thread *start_main_thread(void);

#endif
