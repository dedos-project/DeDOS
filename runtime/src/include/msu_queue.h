/**
 * msu_queue.h
 *
 * Will contain declarations for functions relevant to enqueuing and dequeuing of data to MSUs
 * For the moment, some functionality may be in `generic_msu_queue.h` instead while porting 
 * is underway
 */
#ifndef MSU_QUEUE_H_
#define MSU_QUEUE_H_

// NOTE: Created this file to achieve some sort of privacy on the functions.
// Prior to now there was no .c file for msu queues. 
// -IMP

/**
 * Creates a new MSU queue item and enqueues it on the appropriate MSU.
 * This function is meant to be used only for the introduction of a queue item
 * into the stack, and not for subsequently passing queue items between MSUs
 * (which should just use the return of the msu_receive function)
 */
int initial_msu_enqueue(void *buffer, size_t buffer_len, uint32_t id, struct generic_msu *dest);

#endif
