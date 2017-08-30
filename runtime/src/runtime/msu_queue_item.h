#ifndef MSU_QUEUE_ITEM_H
#define MSU_QUEUE_ITEM_H
#include "message_queue.h"

//TODO: MSU_QUEUE_ITEM
struct msu_queue_item;

struct msu_msg *dequeue_msu_msg(struct msg_queue *q);

#endif
