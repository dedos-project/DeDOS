#ifndef INCLUDE_MSU_TCP_HANDSHAKE
#define INCLUDE_MSU_TCP_HANDSHAKE

#include "pico_addressing.h"
#include "pico_protocol.h"
#include "pico_socket.h"
#include "generic_msu.h"
#include "generic_msu_queue.h"
#include "generic_msu_queue_item.h"
#include "hs_heap.h"
#include "dedos_hs_timers.h"

struct hs_internal_state
{
    heap_hs_timer_ref *hs_timers;
    struct pico_tree *hs_table;
    //can add more internal state to store here
    long int syn_state_used_memory;
    long int syn_state_memory_limit;
    long unsigned int items_dequeued;
    long unsigned int syns_processed;
    long unsigned int syns_dropped;
    long unsigned int synacks_generated;
};

struct hs_queue_item
{
    int reply_msu_id;
    struct pico_frame *f;
};

struct tcp_intermsu_queue_item
{
        int src_msu_id;
            int msg_type;
                int data_len;
                    void *data;
};

struct msu_type TCP_HANDSHAKE_MSU_TYPE;

uint32_t hs_timer_add(heap_hs_timer_ref *timers, pico_time expire,
        void (*timer)(pico_time, void *, void *), void *arg);
void hs_timer_cancel(heap_hs_timer_ref *timers, uint32_t id);
void hs_check_timers(heap_hs_timer_ref *timers);

extern struct pico_protocol msu_tcp_handshake;
struct pico_socket_tcp;
struct pico_tree msu_hs_table;

struct pico_socket *find_tcp_socket(struct pico_sockport *sp,
        struct pico_frame *f);
void* msu_tcp_hs_collect_socket(void *data);
int msu_tcp_hs_restore_socket(struct generic_msu *self,
        struct dedos_intermsu_message* msg, void *buf, uint16_t bufsize);
//int msu_tcp_hs_same_thread_transfer(void *data, void *optional_data);
void buf_to_pico_frame(struct pico_frame **f, void **buf, uint16_t bufsize);
char* pico_frame_to_buf(struct pico_frame *f);
void print_frame(struct pico_frame *f);
int8_t remove_completed_request(struct hs_internal_state *in_state,
        struct pico_socket *s);
int hs_route(struct msu_type *type, struct generic_msu *sender, struct generic_msu_queue_item *data);
int msu_tcp_process_queue_item(struct generic_msu *msu, struct generic_msu_queue_item *queue_item);
#endif


