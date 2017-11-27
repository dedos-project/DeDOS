#ifndef INCLUDE_MSU_TCP_HANDSHAKE
#define INCLUDE_MSU_TCP_HANDSHAKE

#include "pico_addressing.h"
#include "pico_protocol.h"
#include "pico_socket.h"
#include "msu_type.h"
#include "hs_heap.h"
#include "pico_hs_timers.h"

struct tcp_intermsu_queue_item
{
        int src_msu_id;
        int msg_type;
        int data_len;
        void *data;
};

#define TCP_HANDSHAKE_MSU_TYPE_ID  400
struct msu_type TCP_HANDSHAKE_MSU_TYPE;

uint32_t hs_timer_add(heap_hs_timer_ref *timers, pico_time expire,
        void (*timer)(pico_time, void *, void *), void *arg);
void hs_timer_cancel(heap_hs_timer_ref *timers, uint32_t id);
void hs_check_timers(heap_hs_timer_ref *timers);

struct pico_protocol msu_tcp_handshake;
struct pico_socket_tcp;
struct pico_tree msu_hs_table;

char* pico_frame_to_buf(struct pico_frame *f);
void print_frame(struct pico_frame *f);
#endif


