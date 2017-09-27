#include "pico_socket.h"
#include "pico_socket_tcp.h"
#include "pico_ipv4.h"
#include "pico_tcp.h"
#include "pico_eth.h"
#include "pico_config.h"
#include "pico_protocol.h"
#include "pico_tree.h"
//#include "hs_request_routing_msu.h"
#include "msu_message.h"
#include "msu_calls.h"
#include "pico_tcp/msu_tcp_handshake.h"
#include "pico_tcp/msu_pico_tcp.h"
#include "pico_socket_tcp.h"
#include "communication.h"
#include "routing.h"
#include "pico_tcp/pico_hs_timers.h"
#include "pico_tree.h"
#include "pico_stack.h"
#include "logging.h"
#include "local_msu.h"

#define INIT_SOCKPORT { {&LEAF, NULL}, 0, 0 }
#define PROTO(s) ((s)->proto->proto_number)
#define PICO_MIN_MSS (1280)
#define TCP_STATE(s) (s->state & PICO_SOCKET_STATE_TCP)
#define SYN_STATE_MEMORY_LIMIT (1 * 1024 * 1024)


static uint32_t base_tmr_id = 0u;


struct pico_socket *find_tcp_socket(struct pico_sockport *sp,
        struct pico_frame *f);

void* msu_tcp_hs_collect_socket(void *data);
void buf_to_pico_frame(struct pico_frame **f, void **buf, uint16_t bufsize);
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
    long unsigned int syn_established_sock;
    long unsigned int syn_with_sock;
    long unsigned int duplicate_syns_processed;
    long unsigned int acks_received;
    long unsigned int acks_ignored;
    long unsigned int completed_handshakes;
    long unsigned int socket_restore_gen_request_count;
    long unsigned int last_ts; //for storing last ts to measure elapsed time for calling cleanup
};

int8_t remove_completed_request(struct hs_internal_state *in_state,
        struct pico_socket *s);


struct hs_queue_item
{
    int reply_msu_id;
    struct pico_frame *f;
};



volatile pico_time hs_pico_tick;
static long int timers_count;

extern int socket_cmp(void *ka, void *kb);

/*
static int hs_count_sockets(struct pico_tree *hs_table){
    struct pico_sockport *sp;
    struct pico_tree_node *idx_sp, *idx_s;
    int count = 0;
    pico_tree_foreach(idx_sp, hs_table) {
        sp = idx_sp->keyValue;
        if (sp) {
            pico_tree_foreach(idx_s, &sp->socks)
            count++;
        }
    }
    return count;
}
*/

/*
static void print_packet_stats(struct hs_internal_state* in_state){
    printf("HS items_dequeued: %lu\n",in_state->items_dequeued);
    printf("HS syns_processed: %lu\n",in_state->syns_processed);
    printf("HS duplicate_syns_processed: %lu\n",in_state->duplicate_syns_processed);
    printf("HS syn_with_sock: %lu\n",in_state->syn_with_sock);
    printf("HS syn_established_sock: %lu\n",in_state->syn_established_sock);
    printf("HS syns_dropped: %lu\n",in_state->syns_dropped);
    printf("HS synacks_generated: %lu\n",in_state->synacks_generated);
    printf("HS acks received: %lu\n",in_state->acks_received);
    printf("HS acks ignored: %lu\n",in_state->acks_ignored);

    printf("HS handshakes completed: %lu\n", in_state->completed_handshakes);
    printf("HS restore sock req count: %lu\n",in_state->socket_restore_gen_request_count);
    int count;
    count = hs_count_sockets(in_state->hs_table);
    printf("Sockets count: %d\n",count);
    // TODO: exit_flag++;
}
*/

/*
static void dummy_state_cleanup(struct hs_internal_state *in_state){
    static uint64_t last_ts;
    uint64_t ts;
    static int once_full;
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    ts = now.tv_sec;

    if(last_ts == 0){
        last_ts = ts;
    }
    if(in_state->syn_state_used_memory >= in_state->syn_state_memory_limit){
        once_full = 1;
    }
    if(ts - last_ts > 1 && once_full){
        in_state->syn_state_used_memory -= (400 * sizeof(struct pico_tree_node));
        //   printf("used mem: %ld\n", in_state->syn_state_used_memory);
        if(in_state->syn_state_used_memory < 0)
        {
            in_state->syn_state_used_memory = 0;
            log_debug("used mem: %ld\n", in_state->syn_state_used_memory);
        }
        last_ts = ts;
    }
}
*/

uint32_t hs_timer_add(heap_hs_timer_ref *timers, pico_time expire, void (*timer)(pico_time, void *, void*), void *arg){
    struct hs_timer *t = PICO_ZALLOC(sizeof(struct hs_timer));
    struct hs_timer_ref tref;
    /* zero is guard for timers */
    if (base_tmr_id == 0u)
    base_tmr_id++;

    if (!t) {
        log_error("Failed to allocate memory for timer %s","");
        return 0;
    }

    log_debug("Adding timer with fn addr %p arg %p", timer, arg);

    tref.expire = PICO_TIME_MS() + expire;
    t->arg = arg;
    t->timer = timer;
    tref.tmr = t;
    tref.id = base_tmr_id++;
    hs_heap_insert(timers, &tref);
    //    if (timers->n) {
    //         printf("HS Warning: I have %d timers\n", (int)timers->n);
    //    }
    timers_count++;
    return tref.id;
}

void hs_timer_cancel(heap_hs_timer_ref *timers, uint32_t id){
    uint32_t i;
    struct hs_timer_ref *tref = timers->top;
    struct hs_timer_ref tref_unused;
    if (id == 0u){
        return;
    }
    for (i = 1; i <= timers->n; i++) {
        if (tref[i].id == id) {
            PICO_FREE(timers->top[i].tmr);
            timers->top[i].tmr = NULL;
            hs_heap_peek(timers, &tref_unused);
            timers_count--;
            break;
        }
    }
}

void hs_check_timers(heap_hs_timer_ref *timers){

    struct hs_timer *t;
    struct hs_timer_ref tref_unused, *tref = hs_heap_first(timers);
    hs_pico_tick = PICO_TIME_MS();
    while((tref) && (tref->expire < hs_pico_tick)) {
        t = tref->tmr;
        if (t && t->timer){
            t->timer(hs_pico_tick, t->arg, timers);
        }

        if (t)
        {
            PICO_FREE(t);
        }
        t = NULL;
        hs_heap_peek(timers, &tref_unused);
        tref = hs_heap_first(timers);
    }
}


static void send_to_next_msu(struct local_msu *self, unsigned int message_type, char *buf,
                            int bufsize, int reply_msu_id)
{
    struct msu_type *next_msu_type = NULL;
    if (message_type == MSU_PROTO_TCP_HANDSHAKE_RESPONSE) {
        next_msu_type = &PICO_TCP_MSU_TYPE;
    }
    if (message_type == MSU_PROTO_TCP_CONN_RESTORE) {
        next_msu_type = &PICO_TCP_MSU_TYPE;
    }

    struct routing_table *table = get_type_from_route_set(&self->routes, next_msu_type->id);
    struct msu_endpoint endpoint;
    int rtn = get_endpoint_by_id(table, reply_msu_id, &endpoint);
    if (rtn < 0) {
        log_error("Cannot get next endpoint with id %d! Not in routes", reply_msu_id);
        return;
    }


    if (message_type == MSU_PROTO_TCP_HANDSHAKE_RESPONSE) {
        //create a frame struct to enqueue
        struct pico_frame *new_frame = pico_frame_alloc(bufsize);
        memcpy(new_frame->buffer, buf, bufsize);
        new_frame->buffer_len = bufsize;
        new_frame->start = new_frame->buffer;
        new_frame->len = new_frame->buffer_len;
        new_frame->datalink_hdr = new_frame->buffer;
        new_frame->net_hdr = new_frame->datalink_hdr + PICO_SIZE_ETHHDR;
        new_frame->net_len = 20;
        new_frame->transport_hdr = new_frame->net_hdr + new_frame->net_len;
        new_frame->transport_len = (uint16_t) (new_frame->len - new_frame->net_len
                - (uint16_t) (new_frame->net_hdr - new_frame->buffer));
        struct msu_msg_key key;
        int32_t id = pico_tcp_generate_id(self, new_frame);
        set_msg_key(id, &key);
        rtn = init_call_msu_endpoint(self, &endpoint, next_msu_type, &key, bufsize, new_frame);
        if (rtn < 0) {
            log_error("Error calling MSU endpoint for TCP_HANDSHAKE_RESPONSE");
            return;
        }
        //IMP: I AM VERY CONFUSED
        free(new_frame);//just the frame struct buf not the buffer
    } else if (message_type == MSU_PROTO_TCP_CONN_RESTORE) {
        // then payload is just a buffer
        char *restore_buf = malloc(bufsize);
        if (!restore_buf) {
            log_error("Failed to malloc buffer for con restore");
            return;
        }
        memcpy(restore_buf, buf, bufsize);
        struct msu_msg_key key = {};
        rtn = init_call_msu_endpoint(self, &endpoint, next_msu_type, &key, bufsize, restore_buf);
        if (rtn < 0) {
            log_error("Error calling MSU endpoint for TCP_CONN_RESTORE");
            return;
        }
    }
}

static int check_socket_sanity(struct pico_socket *s)
{
    /* checking for pending connections */
    if (TCP_STATE(s) == PICO_SOCKET_STATE_TCP_SYN_RECV) {
        if ((PICO_TIME_MS() - s->timestamp) >= PICO_SOCKET_BOUND_TIMEOUT){
            if(s->remote_addr.ip4.addr == 0x1e00000a){
                printf("removing good client expired sock\n");
            }
            return -1;
        }
    }
    return 0;
}

static int check_socket_established_restore_timeout(struct pico_socket *s){
    if (TCP_STATE(s) == PICO_SOCKET_STATE_TCP_ESTABLISHED) {
        struct pico_socket_tcp *t = (struct pico_socket_tcp *)s;
        if ((PICO_TIME_MS() - t->ack_timestamp) >= PICO_TCP_WAIT_FOR_RESTORE){
            return -1;
        }
    }
    return 0;
}
/*
static int get_loop_score(long int syn_state_used_memory, long int max_limit){
    int loop_score = 0;
    int load = syn_state_used_memory * 10 / max_limit;

    if(load > 8){
        loop_score = 128;
    }
    else if(load > 6){
        loop_score = 64;
    }
    else if(load > 4){
        loop_score = 32;
    }
    else if(load > 2){
        loop_score = 16;
    }
    else{
        loop_score = 8;
    }

    return loop_score;
}
*/
static int pico_sockets_check(struct hs_internal_state *in_state, int loop_score)
{
    struct pico_sockport *start;
    struct pico_socket *s;
    static struct pico_tree_node *index_msu;
    struct pico_sockport *sp_msu = NULL;
    struct pico_tree *msu_table = in_state->hs_table;
    if(pico_tree_empty(msu_table)){
        return 0;
    }
    index_msu = pico_tree_firstNode(msu_table->root);

    sp_msu = index_msu->keyValue;
    start = sp_msu;

    //count = hs_count_sockets(in_state->hs_table);
    //printf("before remove: Sockets count: %d\n",count);
    while (sp_msu != NULL && loop_score > 0) {
        struct pico_tree_node *index = NULL, *safe_index = NULL;
        pico_tree_foreach_safe(index, &sp_msu->socks, safe_index)
        {
            s = index->keyValue;
            //only dec SYN state if stale half open conn
            if (check_socket_sanity(s) < 0) {
                remove_completed_request(in_state, s);
                index_msu = NULL; /* forcing the restart of loop */
                sp_msu = NULL;

                in_state->syn_state_used_memory -= sizeof(struct pico_tree_node);
                log_debug("Decremented syn_state: %ld\n", in_state->syn_state_used_memory);
                if(in_state->syn_state_used_memory < 0){
                    log_warn("syn_state_used_memory: %ld", in_state->syn_state_used_memory);
                    log_critical("Underflow or overflow in syn state size, reset");
                    in_state->syn_state_used_memory = 0;
                }
                break;
            }
            if (check_socket_established_restore_timeout(s) < 0) {
                remove_completed_request(in_state, s);
                index_msu = NULL; /* forcing the restart of loop */
                sp_msu = NULL;
                break;
            }
        }

        if (!index_msu || (index && index->keyValue))
            break;

        index_msu = pico_tree_next(index_msu);
        sp_msu = index_msu->keyValue;

        if (sp_msu == NULL) {
            index_msu = pico_tree_firstNode(msu_table->root);
            sp_msu = index_msu->keyValue;
        }

        if (sp_msu == start)
            break;

        loop_score--;
    }
    //count = hs_count_sockets(in_state->hs_table);
    //printf("after remove: Sockets count: %d\n",count);

    return 0;
}
/*
static int pico_sockets_pending_ack_check(struct hs_internal_state *in_state)
{
    struct pico_sockport *start;
    struct pico_socket *s;
    static struct pico_tree_node *index_msu;
    struct pico_sockport *sp_msu = NULL;
    struct pico_tree *msu_table = in_state->hs_table;
    if(pico_tree_empty(msu_table)){
        return 0;
    }
    index_msu = pico_tree_firstNode(msu_table->root);
    sp_msu = index_msu->keyValue;

    start = sp_msu;
    while (sp_msu != NULL) {
        struct pico_tree_node *index = NULL, *safe_index = NULL;
        pico_tree_foreach_safe(index, &sp_msu->socks, safe_index)
        {
            s = index->keyValue;
            if (check_socket_sanity(s) < 0) {
                remove_completed_request(in_state, s);
                index_msu = NULL; // forcing the restart of loop 
                sp_msu = NULL;
                break;
            }
        }

        if (!index_msu || (index && index->keyValue))
            break;

        index_msu = pico_tree_next(index_msu);
        sp_msu = index_msu->keyValue;

        if (sp_msu == NULL) {
            index_msu = pico_tree_firstNode(msu_table->root);
            sp_msu = index_msu->keyValue;
        }

        if (sp_msu == start)
            break;
    }

    return 0;
}
*/

static struct pico_sockport* find_hs_sockport(struct pico_tree* msu_table, uint16_t local_port)
{
    struct pico_sockport *found = NULL;
    struct pico_sockport test = INIT_SOCKPORT;
    test.number = local_port;
    found = pico_tree_findKey(msu_table, &test);
    return found;
}

static int8_t create_hs_sockport(struct pico_tree* msu_table, uint16_t localport)
{
    /* the request will only reach here if the picoTCP is actually listening on this socket,
    so attacker cannot cause creation of sockports on random ports
    */
    struct pico_sockport *sp = NULL;
    log_debug("Creating sockport..%04x", localport);
    sp = PICO_ZALLOC(sizeof(struct pico_sockport));

    if (!sp) {
        pico_err = PICO_ERR_ENOMEM;
        return -1;
    }
    log_debug("Allocated sockport memory for %u", short_be(localport));
    sp->proto = MSU_PROTO_TCP_HANDSHAKE;
    sp->number = localport;
    sp->socks.root = &LEAF;
    sp->socks.compare = socket_cmp;

    pico_tree_insert(msu_table, sp);
    return 0;
}

int8_t remove_completed_request(struct hs_internal_state *in_state, struct pico_socket *s)
{
    struct pico_tree *msu_table = in_state->hs_table;
    struct pico_sockport *sp = NULL;
    sp = find_hs_sockport(msu_table, s->local_port);

    if (!sp) {
        log_error("Cannot find sockport for port: %u", short_be(s->local_port));
        return -1;
    }

    pico_tree_delete(&sp->socks, s);
    pico_socket_tcp_delete(s);
    s->state = PICO_SOCKET_STATE_CLOSED;
    log_debug("Calling hs_timer_add from remove_completed_request!");
    //hs_socket_garbage_collect(10, s, in_state->hs_timers);
    hs_timer_add(in_state->hs_timers, (pico_time) 10, hs_socket_garbage_collect, s);
    log_debug("done calling hs_timer_add from remove_completed_request!");
/*
    struct pico_tree_node *index;
    log_debug("Current sockets for port %u >>>", short_be(s->local_port));
    pico_tree_foreach(index, &sp->socks)
    {
        s = index->keyValue;
        log_debug(">>>> List Socket lc=%hu rm=%hu", short_be(s->local_port),
                short_be(s->remote_port));
    }
    log_debug("%s", "Removed completed request from socket tree");
*/
    return 0;
}

static inline void tcp_send_add_tcpflags(struct pico_socket_tcp *ts, struct pico_frame *f)
{
    struct pico_tcp_hdr *hdr = (struct pico_tcp_hdr *) f->transport_hdr;
    if (ts->rcv_nxt != 0) {
        if ((ts->rcv_ackd == 0)
                || (pico_seq_compare(ts->rcv_ackd, ts->rcv_nxt) != 0)
                || (hdr->flags & PICO_TCP_ACK)) {
            hdr->flags |= PICO_TCP_ACK;
            hdr->ack = long_be(ts->rcv_nxt);
            ts->rcv_ackd = ts->rcv_nxt;
        }
    }

    if (hdr->flags & PICO_TCP_SYN) {
        ts->snd_nxt++;
    }

    if (f->payload_len > 0) {
        hdr->flags |= PICO_TCP_PSH | PICO_TCP_ACK;
        hdr->ack = long_be(ts->rcv_nxt);
        ts->rcv_ackd = ts->rcv_nxt;
    }
}

static int8_t add_pending_request(struct pico_tree *msu_table, struct pico_socket *s)
{
    struct pico_sockport *sp = NULL;
    sp = find_hs_sockport(msu_table, s->local_port);

    if (!sp) {
        log_error("Cannot find sockport for port: %u", short_be(s->local_port));
        return -1;
    }

    pico_tree_insert(&sp->socks, s);
    s->state |= PICO_SOCKET_STATE_BOUND;
/*
    log_debug("Current sockets for port %u >>>", short_be(s->local_port));
    struct pico_tree_node *index;
    pico_tree_foreach(index, &sp->socks)
    {
        s = index->keyValue;
        log_debug(">>>> List Socket lc=%hu rm=%hu", short_be(s->local_port),
                short_be(s->remote_port));
    }
*/
    return 0;
}

static int hs_tcp_nosync_rst(struct local_msu *self, struct pico_socket *s, struct pico_frame *fr,
        int reply_msu_id)
{
    struct pico_socket_tcp *t = (struct pico_socket_tcp *) s;
    struct pico_frame *f;
    struct pico_tcp_hdr *hdr, *hdr_rcv;
    uint16_t opt_len = tcp_options_size(t, PICO_TCP_RST | PICO_TCP_ACK);
    hdr_rcv = (struct pico_tcp_hdr *) fr->transport_hdr;

    /***************************************************************************/
    /* sending RST */
    f = t->sock.net->alloc(t->sock.net, (uint16_t)(PICO_SIZE_TCPHDR + opt_len));

    if (!f) {
        return -1;
    }

    f->sock = &t->sock;
    hdr = (struct pico_tcp_hdr *) f->transport_hdr;
    hdr->len = (uint8_t)((PICO_SIZE_TCPHDR + opt_len) << 2 | t->jumbo);
    hdr->flags = PICO_TCP_RST | PICO_TCP_ACK;
    hdr->rwnd = short_be(t->wnd);
    tcp_set_space(t);
    tcp_add_options(t, f, PICO_TCP_RST | PICO_TCP_ACK, opt_len);
    hdr->trans.sport = t->sock.local_port;
    hdr->trans.dport = t->sock.remote_port;

    /* non-synchronized state */
    if (hdr_rcv->flags & PICO_TCP_ACK) {
        hdr->seq = hdr_rcv->ack;
    } else {
        hdr->seq = 0U;
    }

    hdr->ack = long_be(SEQN(fr) + fr->payload_len);

    t->rcv_ackd = t->rcv_nxt;
    f->start = f->transport_hdr + PICO_SIZE_TCPHDR;
    hdr->rwnd = short_be(t->wnd);
    hdr->crc = 0;
    hdr->crc = short_be(pico_tcp_checksum(f));
    //copy ip4 stuff to frame from socket
    uint8_t ttl = PICO_IPV4_DEFAULT_TTL;
    uint8_t vhl = 0x45; /* version 4, header length 20 */
    static uint16_t ipv4_progressive_id = 0x91c0;
    uint8_t proto = s->proto->proto_number;
    struct pico_ipv4_hdr *iphdr;
    iphdr = (struct pico_ipv4_hdr *) f->net_hdr;

    iphdr->vhl = vhl;
    iphdr->len = short_be((uint16_t) (f->transport_len + f->net_len));
    iphdr->id = short_be(ipv4_progressive_id);
    iphdr->dst.addr = s->remote_addr.ip4.addr;
    iphdr->src.addr = s->local_addr.ip4.addr;
    iphdr->ttl = ttl;
    iphdr->tos = f->send_tos;
    iphdr->proto = proto;
    iphdr->frag = short_be(PICO_IPV4_DONTFRAG);
    iphdr->crc = 0;
    iphdr->crc = short_be(pico_checksum(iphdr, f->net_len));

    send_to_next_msu(self, MSU_PROTO_TCP_HANDSHAKE_RESPONSE, (char*)f->buffer, f->buffer_len, reply_msu_id);
    return 0;
}
static int dedos_tcp_send_synack(struct local_msu* self, struct pico_socket *s, int reply_msu_id)
{
    struct pico_socket_tcp *ts = TCP_SOCK(s);
    struct pico_frame *synack;
    struct pico_tcp_hdr *hdr;
    uint16_t opt_len = tcp_options_size(ts, PICO_TCP_SYN | PICO_TCP_ACK);
    synack = s->net->alloc(s->net, (uint16_t) (PICO_SIZE_TCPHDR + opt_len));
    if (!synack){
        return -1;
    }
    hdr = (struct pico_tcp_hdr *) synack->transport_hdr;

    synack->sock = s;
    hdr->len = (uint8_t) ((PICO_SIZE_TCPHDR + opt_len) << 2 | ts->jumbo);
    hdr->flags = PICO_TCP_SYN | PICO_TCP_ACK;
    hdr->rwnd = short_be(ts->wnd);
    hdr->seq = long_be(ts->snd_nxt);
    ts->rcv_processed = long_be(hdr->seq);
    ts->snd_last = ts->snd_nxt;
    tcp_set_space(ts);
    tcp_add_options(ts, synack, hdr->flags, opt_len);
    synack->payload_len = 0;
    synack->timestamp = TCP_TIME;

    hdr->trans.sport = ts->sock.local_port;
    hdr->trans.dport = ts->sock.remote_port;
    if (!hdr->seq)
        hdr->seq = long_be(ts->snd_nxt);

    tcp_send_add_tcpflags(ts, synack);

    synack->start = synack->transport_hdr + PICO_SIZE_TCPHDR;
    hdr->rwnd = short_be(ts->wnd);
    hdr->crc = 0;
    hdr->crc = short_be(pico_tcp_checksum(synack));
    //copy ip4 stuff to frame from socket
    uint8_t ttl = PICO_IPV4_DEFAULT_TTL;
    uint8_t vhl = 0x45; /* version 4, header length 20 */
    static uint16_t ipv4_progressive_id = 0x91c0;
    uint8_t proto = s->proto->proto_number;
    struct pico_ipv4_hdr *iphdr;
    iphdr = (struct pico_ipv4_hdr *) synack->net_hdr;

    iphdr->vhl = vhl;
    iphdr->len = short_be((uint16_t) (synack->transport_len + synack->net_len));
    iphdr->id = short_be(ipv4_progressive_id);
    iphdr->dst.addr = s->remote_addr.ip4.addr;
    iphdr->src.addr = s->local_addr.ip4.addr;
    iphdr->ttl = ttl;
    iphdr->tos = synack->send_tos;
    iphdr->proto = proto;
    iphdr->frag = short_be(PICO_IPV4_DONTFRAG);
    iphdr->crc = 0;
    iphdr->crc = short_be(pico_checksum(iphdr, synack->net_len));

    // log_debug("Sending following SYNACK frame to data for forwarding: %s","");
    // print_frame(synack);
    send_to_next_msu(self, MSU_PROTO_TCP_HANDSHAKE_RESPONSE, (char*)synack->buffer, synack->buffer_len, reply_msu_id);

    struct hs_internal_state *in_state = (struct hs_internal_state*) self->msu_state;
    in_state->synacks_generated++;

    pico_frame_discard(synack);
    return 0;
}

static int handle_rst(struct local_msu* self, struct pico_socket *s, struct pico_frame *f,
                        int reply_msu_id){
    struct pico_socket_tcp *t = (struct pico_socket_tcp *) s;
    struct pico_tcp_hdr *hdr = (struct pico_tcp_hdr *) (f->transport_hdr);
    uint32_t this_seq = long_be(hdr->seq);
    if ((this_seq >= t->rcv_ackd) && (this_seq <= ((uint32_t)(short_be(hdr->rwnd) << (t->wnd_scale)) + t->rcv_ackd)))
    {
        (t->sock).state &= 0x00FFU;
        (t->sock).state |= PICO_SOCKET_STATE_TCP_CLOSED;
        (t->sock).state &= 0xFF00U;
        (t->sock).state |= PICO_SOCKET_STATE_CLOSED;
        remove_completed_request(self->msu_state, s);
    }
    return 0;
}

static int handle_syn(struct local_msu* self, struct pico_tree *msu_table, struct pico_frame *f,
                        int reply_msu_id)
{

    /* normally would clone of listen socket
     * here this is the clone that will be copied to data msu after
     * connection establishment
     */
    log_debug("%s", "Handling SYN packet...");
    struct hs_internal_state *in_state = self->msu_state;
    in_state->syns_processed++;

    if(in_state->syn_state_used_memory > in_state->syn_state_memory_limit){
        log_debug("Drop syn full: %ld ", in_state->syn_state_used_memory);
        in_state->syns_dropped++;
        return -10;
    }

    struct pico_socket *s = hs_pico_socket_tcp_open(PICO_PROTO_IPV4, in_state->hs_timers);
    if (!s) {
        log_error("%s", "Error creating local socket");
        return -1;
    }

    struct pico_socket_tcp *new = NULL;
    struct pico_tcp_hdr *hdr = NULL;
    uint16_t mtu;

    s->q_in.max_size = PICO_DEFAULT_SOCKETQ;
    s->q_out.max_size = PICO_DEFAULT_SOCKETQ;
    s->wakeup = NULL; //will know this later during restore from parent listen socket
    // Initialization of empty socket done till here

    log_debug("%s", "Empty socket initialization done...");
    new = (struct pico_socket_tcp *) s;
    hdr = (struct pico_tcp_hdr *) f->transport_hdr;

    new->sock.remote_port = ((struct pico_trans *) f->transport_hdr)->sport;
    new->sock.local_port = ((struct pico_trans *) f->transport_hdr)->dport;
    log_debug("%s", "local and remote port assigned to socket...");

    new->sock.remote_addr.ip4.addr = ((struct pico_ipv4_hdr *) (f->net_hdr))->src.addr;
    new->sock.local_addr.ip4.addr = ((struct pico_ipv4_hdr *) (f->net_hdr))->dst.addr;

    log_debug("remote port : %u, local port: %u", short_be(new->sock.remote_port), short_be(new->sock.local_port));

    f->sock = &new->sock;
    tcp_parse_options(f);
    mtu = (uint16_t) pico_socket_get_mss(&new->sock);
    new->mss = (uint16_t) (mtu - PICO_SIZE_TCPHDR );
    new->tcpq_in.max_size = PICO_DEFAULT_SOCKETQ;
    new->tcpq_out.max_size = PICO_DEFAULT_SOCKETQ;
    new->tcpq_hold.max_size = 2u * mtu;
    new->rcv_nxt = long_be(hdr->seq) + 1;
    new->snd_nxt = long_be(pico_paws());
    new->snd_last = new->snd_nxt;
    new->cwnd = PICO_TCP_IW;
    new->ssthresh = (uint16_t) ((uint16_t) (PICO_DEFAULT_SOCKETQ / new->mss)
            - (((uint16_t) (PICO_DEFAULT_SOCKETQ / new->mss)) >> 3u));
    new->recv_wnd = short_be(hdr->rwnd);
    new->jumbo = hdr->len & 0x07;
    new->linger_timeout = PICO_SOCKET_LINGER_TIMEOUT;
    new->sock.parent = NULL;
    new->sock.wakeup = NULL;
    rto_set(new, PICO_TCP_RTO_MIN);
    /* Initialize timestamp values */
    new->sock.state = PICO_SOCKET_STATE_BOUND | PICO_SOCKET_STATE_CONNECTED | PICO_SOCKET_STATE_TCP_SYN_RECV;

    int stat = add_pending_request(msu_table, &new->sock);
    if(stat == 0){
        in_state->syn_state_used_memory += sizeof(struct pico_tree_node);
        log_debug("SYN state memory used: %ld, cap: %ld\n", in_state->syn_state_used_memory, in_state->syn_state_memory_limit);
    }
    //print_tcp_socket(&new->sock);

    dedos_tcp_send_synack(self, &new->sock, reply_msu_id);
    log_debug("SYNACK sent, socket added. snd_nxt is 0x%08x", new->snd_nxt);

    return 0;
}

static int handle_ack(struct local_msu *self, struct pico_frame *f, struct pico_socket_tcp *t, int reply_msu_id)
{
    struct pico_socket *s = (struct pico_socket *) t;
    struct pico_tcp_hdr *hdr = (struct pico_tcp_hdr *) f->transport_hdr;
    log_debug("Handling ACK, expecting %08x got %08x", t->snd_nxt, ACKN(f));
    struct hs_internal_state *in_state = self->msu_state;

    if (t->snd_nxt == ACKN(f)) {
        tcp_set_init_point(s);
        //print_tcp_socket(&t->sock);
        f->sock = s;
        //hs_tcp_ack(s, f, in_state->hs_timers);
        t->snd_old_ack = ACKN(f);
        //print_tcp_socket(&t->sock);
        //printf("snd_nxt is now %08x", t->snd_nxt);
        //printf("rcv_nxt is now %08x", t->rcv_nxt);
        s->state &= 0x00FFU;
        s->state |= PICO_SOCKET_STATE_TCP_ESTABLISHED;
        in_state->completed_handshakes++;
#if DEBUG != 0
        log_info("TCP Connection established. State: %04x", s->state);
        log_debug("snd_nxt is now %08x", t->snd_nxt);
        log_debug("rcv_nxt is now %08x", t->rcv_nxt);
#endif
    }
    else if ((hdr->flags & PICO_TCP_RST) == 0) {
        hs_tcp_nosync_rst(self, s, f, reply_msu_id);
        remove_completed_request(in_state,  (struct pico_socket *)t);
        return -1;
    }
    else {
        in_state->acks_ignored += 1000;
    }
    return 0;
}
/*
static int handle_bad_con(struct pico_frame *f)
{
    // send reset here
    log_error("%s", "Bad Frame..doing nothing");
    return 0;
}*/

static struct pico_socket *match_tcp_socket(struct pico_socket *s, struct pico_frame *f)
{
    struct pico_socket *found = NULL;
    struct pico_ip4 s_local, s_remote, p_src, p_dst;
    struct pico_ipv4_hdr *ip4hdr = (struct pico_ipv4_hdr*) (f->net_hdr);
    struct pico_trans *tr = (struct pico_trans *) f->transport_hdr;
    s_local.addr = s->local_addr.ip4.addr;
    s_remote.addr = s->remote_addr.ip4.addr;
    p_src.addr = ip4hdr->src.addr;
    p_dst.addr = ip4hdr->dst.addr;
    if ((s->remote_port == tr->sport) && /* remote port check */
        (s_remote.addr == p_src.addr) && /* remote addr check */
        ((s_local.addr == PICO_IPV4_INADDR_ANY) || (s_local.addr == p_dst.addr)))  /* Either local socket is ANY, or matches dst */
    {
        found = s;
        return found;
    }
    else if ((s->remote_port == 0) && /* not connected... listening */
              ((s_local.addr == PICO_IPV4_INADDR_ANY) || (s_local.addr == p_dst.addr))) /* Either local socket is ANY, or matches dst */
    {/* listen socket */
        found = s;
    }

    return found;
}

struct pico_socket *find_tcp_socket(struct pico_sockport *sp, struct pico_frame *f)
{
    struct pico_socket *found = NULL;
    struct pico_tree_node *index = NULL;
    struct pico_tree_node *_tmp;
    struct pico_socket *s = NULL;

    pico_tree_foreach_safe(index, &sp->socks, _tmp)
    {
        s = index->keyValue;
        /* 4-tuple identification of socket (port-IP) */
        if (IS_IPV4(f)) {
            found = match_tcp_socket(s, f);
        }

        if (found)
            break;
    } /* FOREACH */

    return found;
}



static int hs_tcp_send_rst(struct local_msu *self, struct pico_socket *s, struct pico_frame *fr,
        int reply_msu_id)
{

    struct pico_socket_tcp *t = (struct pico_socket_tcp *) s;
    struct pico_tcp_hdr *hdr_rcv;
    int ret = 0;

    /* non-synchronized state, go to CLOSED here to prevent timer callback to go on after timeout */
    (t->sock).state &= 0x00FFU;
    (t->sock).state |= PICO_SOCKET_STATE_TCP_CLOSED;
    // ret = tcp_do_send_rst(s, long_be(t->snd_nxt));
    // all the reset frame generation is done here
    hdr_rcv = (struct pico_tcp_hdr *) fr->transport_hdr;
    int seq = hdr_rcv->ack;

    uint16_t opt_len = tcp_options_size(t, PICO_TCP_RST);
    struct pico_frame *f;
    struct pico_tcp_hdr *hdr;
    f = t->sock.net->alloc(t->sock.net, (uint16_t)(PICO_SIZE_TCPHDR + opt_len));
    if (!f) {
        return -1;
    }
    f->sock = &t->sock;
    log_debug("TCP SEND_RST >>>>>>>>>>>>>>> START\n");

    hdr = (struct pico_tcp_hdr *) f->transport_hdr;
    hdr->len = (uint8_t)((PICO_SIZE_TCPHDR + opt_len) << 2 | t->jumbo);
    hdr->flags = PICO_TCP_RST;
    hdr->rwnd = short_be(t->wnd);
    tcp_set_space(t);
    tcp_add_options(t, f, PICO_TCP_RST, opt_len);
    hdr->trans.sport = t->sock.local_port;
    hdr->trans.dport = t->sock.remote_port;
    hdr->seq = seq;
    hdr->ack = long_be(t->rcv_nxt);
    t->rcv_ackd = t->rcv_nxt;
    f->start = f->transport_hdr + PICO_SIZE_TCPHDR;
    hdr->rwnd = short_be(t->wnd);
    hdr->crc = 0;
    hdr->crc = short_be(pico_tcp_checksum(f));
    //copy ip4 stuff to frame from socket
    uint8_t ttl = PICO_IPV4_DEFAULT_TTL;
    uint8_t vhl = 0x45; /* version 4, header length 20 */
    static uint16_t ipv4_progressive_id = 0x91c0;
    uint8_t proto = s->proto->proto_number;
    struct pico_ipv4_hdr *iphdr;
    iphdr = (struct pico_ipv4_hdr *) f->net_hdr;

    iphdr->vhl = vhl;
    iphdr->len = short_be((uint16_t) (f->transport_len + f->net_len));
    iphdr->id = short_be(ipv4_progressive_id);
    iphdr->dst.addr = s->remote_addr.ip4.addr;
    iphdr->src.addr = s->local_addr.ip4.addr;
    iphdr->ttl = ttl;
    iphdr->tos = f->send_tos;
    iphdr->proto = proto;
    iphdr->frag = short_be(PICO_IPV4_DONTFRAG);
    iphdr->crc = 0;
    iphdr->crc = short_be(pico_checksum(iphdr, f->net_len));

    send_to_next_msu(self, MSU_PROTO_TCP_HANDSHAKE_RESPONSE, (char*)f->buffer, f->buffer_len, reply_msu_id);

    /* Set generic socket state to CLOSED, too */
    (t->sock).state &= 0xFF00U;
    (t->sock).state |= PICO_SOCKET_STATE_CLOSED;

    return ret;
}

static int msu_process_hs_request_in(struct local_msu *self, int reply_msu_id, struct pico_frame *f)
{
    //DONT NOT FREE THE INCOMING FRAME HERE
    int ret = -1;
    struct hs_internal_state *in_state = self->msu_state;
    if (!in_state->hs_table) {
        log_error("%s", "No internal state found!");
        goto end;
    }
    /* check what flags are set and take appropriate action */
    struct pico_tcp_hdr *hdr = (struct pico_tcp_hdr *) (f->transport_hdr);
    struct pico_trans *trans_hdr = (struct pico_trans *) f->transport_hdr;
    uint8_t flags = hdr->flags;

    // Check if sockport exists for incoming frame, if not create one
    struct pico_sockport *sp = NULL;
    sp = find_hs_sockport(in_state->hs_table, trans_hdr->dport);
    if (!sp) {
        ret = create_hs_sockport(in_state->hs_table, trans_hdr->dport);
        if (ret) {
            log_error("Failed to create sockport for %u", short_be(trans_hdr->dport));
            goto end2;
        }
        log_debug("Sockport creation complete %u", short_be(trans_hdr->dport));
        sp = find_hs_sockport(in_state->hs_table, trans_hdr->dport);
    }

    //Check if there is an existing socket associated with this
    struct pico_socket_tcp *tcp_sock_found = NULL;
    struct pico_socket *sock_found = find_tcp_socket(sp, f);

    tcp_sock_found = (struct pico_socket_tcp *) sock_found;
    if(flags == PICO_TCP_ACK || flags == PICO_TCP_PSHACK){
        in_state->acks_received++;
    }

    // take care socket found or not based of flags and current state of socket
    if (flags == PICO_TCP_SYN && !tcp_sock_found) /* First SYN */
    {   // first syn
        ret = handle_syn(self, in_state->hs_table, f, reply_msu_id);
        if (ret != 0 && ret != -10) { log_error("%s", "Failed to handle_syn"); }
    }
    else if (flags == PICO_TCP_SYN && tcp_sock_found) /* Duplicate SYN */
    {
        log_debug("Received SYN duplicate %s","");
        if (sock_found->state == (PICO_SOCKET_STATE_BOUND | PICO_SOCKET_STATE_TCP_SYN_RECV |
                                  PICO_SOCKET_STATE_CONNECTED))
        {
            in_state->syn_with_sock++;
            if (tcp_sock_found->rcv_nxt == long_be(hdr->seq) + 1u) {
                /* take back our own SEQ number to its original value, so the synack retransmitted
                is identical to the original. */
                tcp_sock_found->snd_nxt--;
                in_state->duplicate_syns_processed++;
                log_debug("Sending original SYN ACK %s", "");
                dedos_tcp_send_synack(self, &tcp_sock_found->sock, reply_msu_id);
            }
            else
            {
                log_warn("Bad seqs...cleaning up socket..%s","");
                hs_tcp_send_rst(self, sock_found, f, reply_msu_id);

                /* non-synchronized state, go to CLOSED here to prevent timer callback to go on after timeout */
                //(tcp_sock_found->sock).state &= 0x00FFU;
                //(tcp_sock_found->sock).state |= PICO_SOCKET_STATE_TCP_CLOSED;
                // ret = tcp_do_send_rst(s, long_be(t->snd_nxt));
                /* Set generic socket state to CLOSED, too */
                //(tcp_sock_found->sock).state &= 0xFF00U;
                //(tcp_sock_found->sock).state |= PICO_SOCKET_STATE_CLOSED;

                remove_completed_request(in_state,  (struct pico_socket *)tcp_sock_found);
                goto end;
            }
        }

        else if (sock_found->state  == (PICO_SOCKET_STATE_BOUND | PICO_SOCKET_STATE_TCP_ESTABLISHED | PICO_SOCKET_STATE_CONNECTED)) {
            in_state->syn_established_sock++;
            //struct pico_trans *tr = (struct pico_trans *) f->transport_hdr;
        //    printf("Established socket SYN for sport: %u\n",short_be(tr->sport));
            log_warn("SYN for established socket..ignoring..%s","");
            goto end;
        }

    }
    else if ((flags == PICO_TCP_ACK || flags == PICO_TCP_PSHACK ) && sock_found) /* ACK for prev seen SYN */
    {
        log_debug("Recieved an ACK, with a known socket %s", "");
        if (sock_found->state == (PICO_SOCKET_STATE_BOUND | PICO_SOCKET_STATE_TCP_SYN_RECV |
                                  PICO_SOCKET_STATE_CONNECTED)) /* First ACK */
        {
            log_debug("%s", "This is the first ack");
            int stat = handle_ack(self, f, tcp_sock_found, reply_msu_id);
            if(stat == 0){
                in_state->syn_state_used_memory -= sizeof(struct pico_tree_node);
                log_warn("Syn state decremented on ACK: %ld", in_state->syn_state_used_memory);
                if(in_state->syn_state_used_memory < 0){
                    log_critical("Underflow or overflow in syn state size, reset");
                    in_state->syn_state_used_memory = 0;
                }
/*
#ifdef PICO_SUPPORT_SAME_THREAD_TCP_MSUS
            {
                //create a new clone socket to add
                struct pico_socket *s = pico_socket_tcp_open(PICO_PROTO_IPV4);
                struct pico_socket_tcp *new = (struct pico_socket_tcp *)s;

                struct pico_socket *old = &tcp_sock_found->sock;

                if (!s) {
                    log_error("%s", "Error creating local socket");
                    return -1;
                }
                struct pico_tcp_hdr *hdr = NULL;

                hdr = (struct pico_tcp_hdr *) f->transport_hdr;

                memcpy(&s->local_addr, &old->local_addr, sizeof(struct pico_ip4));
                memcpy(&s->remote_addr, &old->remote_addr, sizeof(struct pico_ip4));

                s->local_port = old->local_port;
                s->remote_port = old->remote_port;

                s->ev_pending = old->ev_pending;
                s->max_backlog = old->max_backlog;
                s->number_of_pending_conn = old->number_of_pending_conn;

                s->state = old->state;
                s->opt_flags = old->opt_flags;
                s->timestamp = old->timestamp;

                //copy tcp socket data
                memcpy(&new->snd_nxt, &tcp_sock_found->snd_nxt,
                        sizeof(struct pico_socket_tcp)
                                - (sizeof(struct pico_socket) +
                                    (3 * sizeof(struct pico_tcp_queue))
                                ));


                //call the same thread restore here
                msu_tcp_hs_same_thread_transfer(new, f);
                ret = remove_completed_request(in_state,  (struct pico_socket *)tcp_sock_found);
                goto end2;
            }
#endif
*/
                log_debug("Collecting socket info %s","");
                struct pico_socket_tcp_dump *collected_tcp_sock = msu_tcp_hs_collect_socket((void*) sock_found);

                //send to next msu
                send_to_next_msu(self, MSU_PROTO_TCP_CONN_RESTORE,
                        (char*)collected_tcp_sock, sizeof(struct pico_socket_tcp_dump), reply_msu_id);

                //free collected_tcp_sock after sending the buff
                free(collected_tcp_sock);
                remove_completed_request(in_state, (struct pico_socket *)tcp_sock_found);
            } //if stat == 0
        }
        //Duplicate first ACK
        else if(sock_found->state == (PICO_SOCKET_STATE_BOUND | PICO_SOCKET_STATE_TCP_ESTABLISHED |
                                                                PICO_SOCKET_STATE_CONNECTED))
        {
            log_warn("Duplicate first ACK %s. IGNORING FOR NOW","");
            /* take back our own SEQ number to its original value
             * faking to client since we don't want to lose anytime in the meantime
             * when connection is being restored
             * so the synack retransmitted is identical to the original.
             */
            // tcp_sock_found->snd_nxt--;
            // log_debug("Sending original SYN ACK for keeping TCP happy..%s", "");
            // here now send the original SYN/ACK back so that the client again sends the first ACK,
            //and if the connection has been restored in data MSU, that that will take take of duplicate,
            //first ACK and all things will be happy thereafter.
            // dedos_tcp_send_synack(self, &tcp_sock_found->sock, reply_msu_id);
        }
    }
    else if ((flags == PICO_TCP_ACK || flags == PICO_TCP_PSHACK) && !sock_found) {
        in_state->acks_ignored++;
    	log_warn("ACK with no associated socket %s","");
    }
    else if ((flags == PICO_TCP_RST || flags == PICO_TCP_PSHACK) && sock_found) {
        handle_rst(self, sock_found, f, reply_msu_id);
    }
    else
    {
        log_warn("%s", "Unexpected flags set.or a random ack. Ignoring");
        log_warn("Ignoring frame %s", "");
    }
end2:
end:
/** Now called in msu_process_queue_item anyway
    pico_sockets_pending_ack_check(in_state); //for connection that only sent SYN
    pico_sockets_pending_restore_check(in_state); //for sockets pending restore
*/
    return ret;
}

void* msu_tcp_hs_collect_socket(void *data)
{
    struct pico_socket *s = (struct pico_socket*) data;
    struct pico_socket_tcp *t = (struct pico_socket_tcp*) s;

    struct pico_socket_tcp_dump *t_dump = PICO_ZALLOC(sizeof(struct pico_socket_tcp_dump));
    struct pico_socket_dump *s_dump = (struct pico_socket_dump*) t_dump;

    //copy pico_socket data
    s_dump->l4_proto = s->proto->proto_number;
    s_dump->l3_net = s->net->proto_number;

    memcpy(&s_dump->local_addr, &s->local_addr, sizeof(struct pico_ip4));
    memcpy(&s_dump->remote_addr, &s->remote_addr, sizeof(struct pico_ip4));

    s_dump->local_port = s->local_port;
    s_dump->remote_port = s->remote_port;

    s_dump->ev_pending = s->ev_pending;
    s_dump->max_backlog = s->max_backlog;
    s_dump->number_of_pending_conn = s->number_of_pending_conn;

    s_dump->state = s->state;
    s_dump->opt_flags = s->opt_flags;
    s_dump->timestamp = s->timestamp;

    //copy tcp socket data
    memcpy(&t_dump->snd_nxt, &t->snd_nxt,
            sizeof(struct pico_socket_tcp_dump)
                    - sizeof(struct pico_socket_dump));

    //debug queues to see if they have data
    //print_tcp_socket_queues(s);

    //return pointer to serialized buffer
    log_debug("%s", "Collected Socket");
    return (void*) t_dump;
}
/*
static struct pico_socket *match_tcp_sockets(struct pico_socket *s1, struct pico_socket *s2)
{
    struct pico_socket *found = NULL;
    if ((s1->remote_port == s2->remote_port) && // remote port check
        (s1->remote_addr.ip4.addr == s2->remote_addr.ip4.addr) && // remote addr check 
            ((s1->local_addr.ip4.addr == PICO_IPV4_INADDR_ANY)
                    || (s1->local_addr.ip4.addr == s2->local_addr.ip4.addr))) { // Either local socket is ANY, or matches dst 
        found = s1;
        return found;
    } else if ((s1->remote_port == 0) && not connected... listening
    (s1->local_port == s2->local_port)) {  Either local socket is ANY, or matches dst
        // listen socket
        found = s1;
    }

    return found;
}
*/
/*
static struct pico_socket *find_listen_sock(struct pico_sockport *sp, struct pico_socket *sock)
{
    struct pico_socket *found = NULL;
    struct pico_tree_node *index = NULL;
    struct pico_tree_node *_tmp;
    struct pico_socket *s = NULL;

    pico_tree_foreach_safe(index, &sp->socks, _tmp)
    {
        s = index->keyValue;
        // 4-tuple identification of socket (port-IP) 
        if (sock->net->proto_number == PICO_PROTO_IPV4) {
            found = match_tcp_sockets(s, sock);
        }

        if (found)
            break;
    } // FOREACH

    return found;
}
*/

char* pico_frame_to_buf(struct pico_frame *f)
{
    return (char*) f->buffer;

    log_debug("f: %p", f);
    log_debug("f->buffer : %p", f->buffer);
    log_debug("f->buffer_len: %u", f->buffer_len);
    log_debug("f->start: %p", f->start);
    log_debug("f->len: %u", f->len);
    log_debug("f->datalink_hdr: %p", f->datalink_hdr);
    log_debug("f->net_hdr: %p", f->net_hdr);
    log_debug("f->net_len: %u", f->net_len);
    log_debug("f->transport_hdr: %p", f->transport_hdr);
    log_debug("f->transport_len: %u", f->transport_len);

}

void buf_to_pico_frame(struct pico_frame **f, void **buf, uint16_t bufsize)
{
    *f = *buf;
    (*f)->buffer = (*f)->start = *buf;
    (*f)->buffer_len = (*f)->len = bufsize;
    (*f)->datalink_hdr = *buf;
    (*f)->net_hdr = (*f)->buffer + PICO_SIZE_ETHHDR + 2;
    (*f)->net_len = 20;
    (*f)->transport_hdr = (*f)->buffer + 34;
    (*f)->transport_len = (uint16_t) ((*f)->len - (*f)->net_len
            - (uint16_t) ((*f)->net_hdr - (*f)->buffer));
}

void print_frame(struct pico_frame *f)
{
    struct pico_tcp_hdr *hdr = (struct pico_tcp_hdr *) (f->transport_hdr);

    f->payload = (f->transport_hdr + ((hdr->len & 0xf0u) >> 2u));
    f->payload_len = (uint16_t) (f->transport_len - ((hdr->len & 0xf0u) >> 2u));
    log_debug("> frame_buffer_len %u", f->buffer_len);
    log_debug("> [tcp input] t_len: %u", f->transport_len);
    log_debug("> flags = 0x%02x", hdr->flags);
    log_debug(
            "> dst port:%u src port: %u seq: 0x%08x ack: 0x%08x flags: 0x%02x t_len: %u, hdr: %u payload: %d",
            short_be(hdr->trans.dport), short_be(hdr->trans.sport), SEQN(f),
            ACKN(f), hdr->flags, f->transport_len, (hdr->len & 0xf0) >> 2,
            f->payload_len);

    struct pico_ipv4_hdr *net_hdr = (struct pico_ipv4_hdr *) f->net_hdr;
    char peer[30], local[30];
    pico_ipv4_to_string(peer, net_hdr->src.addr);
    pico_ipv4_to_string(local, net_hdr->dst.addr);
    log_debug("Src addr %s: dst addr : %s", peer, local);
}
/*
static void handle_incoming_frame(struct local_msu *self, int reply_dst_msu_id, struct pico_frame *f)
{
    int ret = 0;
    struct pico_tcp_hdr *hdr = (struct pico_tcp_hdr *) (f->transport_hdr);
    //struct pico_ipv4_hdr *iphdr = (struct pico_ipv4_hdr *) f->net_hdr;
    uint8_t flags = hdr->flags;

    if (flags == (PICO_TCP_SYN | PICO_TCP_ACK)) {
        //forwarding logic only relevant at data MSU
        log_warn("%s", "HS Shouldn't be getting a SYNACK FRAME!");
    } else {
        log(LOG_PICO, "Non SYNACK frame, enqueuing for processing");
        struct hs_queue_item *queue_payload = malloc(sizeof(*queue_payload));
        if(!queue_payload){
            log_error("Failed to allocate memory for hs_queue_item");
            return;
        }
        queue_payload->f = f;
        queue_payload->reply_msu_id = reply_dst_msu_id;
        struct msu_msg_key key = {};
        ret = init_call_local_msu(self, self, &key, f->buffer_len + sizeof(*queue_payload), queue_payload);
        log(LOG_PICO, "Enqueued MSU_PROTO_TCP_HANDSHAKE request: %d", ret);
    }
}
*/
/*
static int valid_ip_tcp_flags(struct pico_frame *f)
{
    struct pico_tcp_hdr *hdr = (struct pico_tcp_hdr *) (f->transport_hdr);
    uint8_t flags = hdr->flags;
    if (IS_IPV4(f)
        && (flags == PICO_TCP_SYN || flags == (PICO_TCP_SYN | PICO_TCP_ACK)
        || flags == PICO_TCP_ACK))
    {
        return 0; //if valid
    } else {
        return -1;
    }
}
*/
//IMP: I don't think this is necessary anymore
/*
void *msu_hs_deserialize(struct local_msu *self, size_t input_size, void *input, size_t *out_size) {
    //clone the buf
    void *cloned_buffer = malloc(input_size);
    memcpy(cloned_buffer, input, input_size);

    struct tcp_intermsu_queue_item *tcp_queue_item = cloned_buffer;
    log(LOG_PICO, "src msu id: %d",tcp_queue_item->src_msu_id);
    log(LOG_PICO, "msg type: %d",tcp_queue_item->msg_type);
    log(LOG_PICO, "data len: %d",tcp_queue_item->data_len);

    if(tcp_queue_item->msg_type != MSU_PROTO_TCP_HANDSHAKE){
        log_error("Unexpected msg type: %d", tcp_queue_item->msg_type);
        free(cloned_buffer);
        return NULL;
    }

    log(LOG_PICO, "Recvd bufsize: %d", (int)input_size);
    log(LOG_PICO, "First frame len: %d", (int)tcp_queue_item->data_len);
    log(LOG_PICO, "tcp_intermsu_queue_item size: %d", (int)sizeof(struct tcp_intermsu_queue_item));
    if(input_size > tcp_queue_item->data_len + sizeof(struct tcp_intermsu_queue_item)){
        log_critical("MULTIPLE messages in same tcp transmission!, HANDLE THIS");
    }

    *out_size = input_size;
    free(input);
    return cloned_buffer;
}
*/

int msu_tcp_hs_same_thread_transfer(void *data, void *optional_data)
{
    log_debug("%s", "Same host established conn socket transfer");

    struct pico_socket *sock_found = (struct pico_socket *) data;
    struct pico_socket_tcp *tcp_sock_found =
            (struct pico_socket_tcp *) sock_found;
    struct pico_sockport *data_sp = pico_get_sockport(PICO_PROTO_TCP,
            sock_found->local_port);
    struct pico_frame *f = (struct pico_frame*) optional_data;
    log_debug("Found sockport in TCP data sockport tree%s","");
    if (!data_sp) {
        log_error("No such port %u", short_be(sock_found->local_port));
        return -1;
    }

    struct pico_socket *listen_sock = find_tcp_socket(data_sp, f);
    if (!listen_sock) {
        log_error("No listen sock in orig %s", "");
    }
    log_debug("Found listen sock in data transfer msu%s","");
    //tcp_sock_found would be the restored socket
    tcp_sock_found->sock.parent = listen_sock;
    tcp_sock_found->sock.wakeup = listen_sock->wakeup;

    pico_socket_add(&tcp_sock_found->sock);
    tcp_sock_found->retrans_tmr = 0;

    tcp_send_ack(tcp_sock_found);

    if (sock_found->parent && sock_found->parent->wakeup) {
        log_debug("FIRST ACK - Parent found -> listening socket %u",
                short_be(sock_found->local_port));
        sock_found->parent->wakeup(PICO_SOCK_EV_CONN, sock_found->parent);
    }
    sock_found->ev_pending |= PICO_SOCK_EV_WR;

    return 0;
}

static inline long unsigned int get_ts(){
    struct timeval st;
    long unsigned int ts_usec;
    gettimeofday(&st, CLOCK_REALTIME);
    ts_usec =  st.tv_sec * 1000000 + st.tv_usec;

    return ts_usec;
}

static int msu_tcp_process_queue_item(struct local_msu *msu, struct msu_msg *msg)
{
    //interface from runtime to picoTCP structures or any other existing code

    // TODO: Post to msu thread's semaphore
    /*
    msu_queue *q_data = &msu->q_in;
    int rtn = sem_post(q_data->thread_q_sem);
    if(rtn < 0){
        log_error("Failed to increment semaphore for handshake msu");
    }
    */

    struct hs_internal_state *in_state = msu->msu_state;

    if(msg->data){
        in_state->items_dequeued++;
        struct tcp_intermsu_queue_item *tcp_queue_item = msg->data;
        int frame_len = tcp_queue_item->data_len;

        tcp_queue_item->data = ((char*)msg->data) + sizeof(*tcp_queue_item);

        /* Following moved to deserialize to prevent enqueue of bad item */
        // if(msg_type != MSU_PROTO_TCP_HANDSHAKE){
        //     log_error("Unexpected msg type: %d",msg_type);
        //     delete_generic_msu_queue_item(queue_item);
        // }

        struct pico_frame *f = pico_frame_alloc(frame_len);
        if(!f || !f->buffer){
            log_error("Failed to allocate pico frame");
            return -1;
        }
        memcpy(f->buffer, tcp_queue_item->data, frame_len);

        //Fix frame pointers
        f->buffer_len = frame_len;
        f->start = f->buffer;
        f->len = f->buffer_len;
        f->datalink_hdr = f->buffer;
        f->net_hdr = f->datalink_hdr + PICO_SIZE_ETHHDR;
        f->net_len = 20;
        f->transport_hdr = f->net_hdr + f->net_len;
        f->transport_len = (uint16_t) (f->len - f->net_len - (uint16_t) (f->net_hdr - f->buffer));
        log_debug("Fixed frame pointers..");

#ifdef DATAPLANE_PROFILING
        log_dp_event(msu->id, DEDOS_SINK, &queue_item->dp_profile_info);
#endif
        msu_process_hs_request_in(msu, tcp_queue_item->src_msu_id, f);

        pico_frame_discard(f);
        free(msg->data);
    }
    // TODO: exit_flag
    /*
    if(exit_flag == 1){
        print_packet_stats(msu->internal_state);
    }
    */

    //pico_sockets_pending_ack_check(in_state); //for connection that only sent SYN
    pico_rand_feed(32);
    //TODO 1) make the following calls based on elapsed time

    long unsigned int cur_ts_usec = get_ts();
    //TODO 2_ play around with loop score and see the numbers (diff in microseconds)
    if(cur_ts_usec - in_state->last_ts > 10 * 1000){
        //printf("Tick: %lu - %lu = %lu\n", cur_ts_usec, in_state->last_ts, cur_ts_usec - in_state->last_ts);
        in_state->last_ts = cur_ts_usec;
        pico_sockets_check(in_state, 32); //for sockets pending restore
        hs_check_timers(in_state->hs_timers);
    }

/*
    if(in_state->items_dequeued % (msu->scheduling_weight*1000) == 0){
        pico_sockets_check(in_state, 32); //for sockets pending restore
    }
    if(in_state->items_dequeued % (msu->scheduling_weight*1000) == 0){
        hs_check_timers(in_state->hs_timers);
    }
*/
    //dummy_state_cleanup(in_state);

    return -10;
}

static void msu_tcp_handshake_destroy(struct local_msu *self)
{
    log_debug("Destroying MSU with id: %u", self->id);
    struct hs_internal_state *in_state = self->msu_state;

    free(in_state->hs_table);
    log_debug("%s","Freed hs_table");

    free(in_state->hs_timers);
    log_debug("%s","Freed hs_timers");

    free(self->msu_state);
    log_debug("%s","Freed internal state");
}

static struct local_msu *HANDSHAKE_MSU;

static int msu_tcp_handshake_init(struct local_msu *self, struct msu_init_data *data) {
    if (HANDSHAKE_MSU != NULL) {
        log_warn("Handshake MSU already initialized");
    }
    HANDSHAKE_MSU = self;

    HANDSHAKE_MSU->scheduling_weight = 128;
    struct hs_internal_state *in_state = malloc(sizeof(*in_state));
    if(!in_state){
        return -1;
    }
    struct pico_tree *hs_table = malloc(sizeof(*hs_table));
    if (!(hs_table)) {
        log_error("Failed to allocate internal struct pico_tree");
        free(in_state);
        return -1;
    }

    hs_table->root = &LEAF;
    hs_table->compare = sockport_cmp;

    /* Initialize timer heap */
    in_state->hs_timers = hs_heap_init();
    if (!in_state->hs_timers){
        log_error("%s", "Failed init timer heap");
        free(in_state);
        free(hs_table);
        return -1;
    }
    in_state->hs_table = hs_table;
    in_state->syn_state_used_memory = 0;
    in_state->syn_state_memory_limit = SYN_STATE_MEMORY_LIMIT;
    in_state->items_dequeued = 0;
    in_state->syns_dropped = 0;
    in_state->synacks_generated = 0;
    in_state->syn_established_sock = 0;
    in_state->syns_processed = 0;
    in_state->syn_with_sock = 0;
    in_state->duplicate_syns_processed = 0;
    in_state->acks_received = 0;
    in_state->acks_ignored = 0;
    in_state->completed_handshakes = 0;
    in_state->socket_restore_gen_request_count = 0;
    in_state->last_ts = 0;
    HANDSHAKE_MSU->msu_state = in_state;

    log(LOG_PICO, "Created %s MSU with id: %u", self->type->name, self->id);

    struct msu_msg_key key = {};
    int rtn = init_call_local_msu(self, self, &key, 0, NULL);
    if (rtn < 0) {
        log_error("Error calling self!");
        return -1;
    }
    return 0;
}

// IMP: I don't think hs_route is doing anything?
/*
int hs_route(struct msu_type *type, struct generic_msu *sender, struct generic_msu_queue_item *queue_item){
    struct tcp_intermsu_queue_item *tcp_queue_item;
    struct msu_endpoint *ret_endpoint = NULL;
    tcp_queue_item = (struct tcp_intermsu_queue_item*)queue_item->buffer;
    tcp_queue_item->data = queue_item->buffer + sizeof(struct tcp_intermsu_queue_item);
    int frame_len = tcp_queue_item->data_len;

    struct pico_frame *f = (struct pico_frame *)malloc(sizeof(struct pico_frame));
    if(!f){
        log_error("Failed malloc pico frame");
        return -1;
    }
    f->buffer = tcp_queue_item->data;
    f->buffer_len = frame_len;
    f->start = tcp_queue_item->data;
    f->len = f->buffer_len;
    f->datalink_hdr = f->buffer;
    f->net_hdr = f->datalink_hdr + PICO_SIZE_ETHHDR;
    f->net_len = 20;
    f->transport_hdr = f->net_hdr + f->net_len;
    f->transport_len = (uint16_t) (f->len - f->net_len - (uint16_t) (f->net_hdr - f->buffer));
    struct pico_ipv4_hdr *net_hdr = (struct pico_ipv4_hdr *) f->net_hdr;
    struct pico_tcp_hdr *tcp_hdr = (struct pico_tcp_hdr *) (f->transport_hdr);
    //log_debug("dst port:%u src port: %u ",
    //        short_be(tcp_hdr->trans.dport), short_be(tcp_hdr->trans.sport));

    //char peer[30], local[30];
    //pico_ipv4_to_string(peer, net_hdr->src.addr);
    //pico_ipv4_to_string(local, net_hdr->dst.addr);
    //log_debug("Src addr %s: dst addr : %s", peer, local);

    //log_debug("calling rr with 4 tup");
    //ret_endpoint = round_robin_with_four_tuple(type, sender,
    //                                net_hdr->src.addr, short_be(tcp_hdr->trans.sport),
    //                                net_hdr->dst.addr, short_be(tcp_hdr->trans.dport));
    free(f);
    return ret_endpoint;
}
*/

struct msu_type TCP_HANDSHAKE_MSU_TYPE = {
    .name="tcp_handshake_msu",
    .id=TCP_HANDSHAKE_MSU_TYPE_ID,
    .init=msu_tcp_handshake_init,
    .destroy=msu_tcp_handshake_destroy,
    .receive=msu_tcp_process_queue_item,
};
