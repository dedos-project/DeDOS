/**
 * @file output_thread.h
 *
 * A dedos_thread which monitors a queue for output to be sent to
 * other runtimes or the global controller
 */
#ifndef OUTPUT_THREAD_H_
#define OUTPUT_THREAD_H_

#include "dedos_threads.h"
#include "thread_message.h"

/**
 * A thread_msg marked for delivery to this thread ID will be enqueued to the output thread
 */
#define OUTPUT_THREAD_ID -1

/**
 * Enqueues a thread_msg for delivery to the output thread.
 * Message should be of type ::CONNECT_TO_RUNTIME, ::SEND_TO_PEER, or ::SEND_TO_CTRL.
 *
 * @return 0 on success, -1 on error
 */
int enqueue_for_output(struct thread_msg *thread_msg);

/** Triggers the output thread to stop execution */
void stop_output_monitor();
/** Joins the underlying pthread */
void join_output_thread();

/** Starts the thread monitoring the queue for messages to be sent to other endpoints */
struct dedos_thread *start_output_monitor_thread(void);

#endif
