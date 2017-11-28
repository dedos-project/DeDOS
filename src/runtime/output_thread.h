/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
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
