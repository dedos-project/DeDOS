#ifndef MSU_PICO_TCP_H
#define MSU_PICO_TCP_H

#include "generic_msu.h"
#include "generic_msu_queue.h"
#include "generic_msu_queue_item.h"
#include "pico_frame.h"

struct msu_type PICO_TCP_MSU_TYPE;
struct generic_msu *pico_tcp_msu;
int pico_tcp_generate_id(struct generic_msu *self, struct pico_frame *f);

#endif
