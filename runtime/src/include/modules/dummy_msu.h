#ifndef DUMMY_MSU_H_
#define DUMMY_MSU_H_

#include "generic_msu.h"
#include "generic_msu_queue_item.h"

#define DEDOS_DUMMY_MSU 888

int dummy_msu_process_queue_item(msu_t *msu, msu_queue_item_t *queue_item);

msu_type_t DUMMY_MSU_TYPE;
#endif /* DUMMY_MSU_H_ */
