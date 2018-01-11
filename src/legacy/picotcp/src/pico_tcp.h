/*********************************************************************
   PicoTCP. Copyright (c) 2012-2015 Altran Intelligent Systems. Some rights reserved.
   See LICENSE and COPYING for usage.

   .

 *********************************************************************/
#ifndef INCLUDE_PICO_TCP
#define INCLUDE_PICO_TCP
#include "unused_def.h"
#include "pico_addressing.h"
#include "pico_protocol.h"
#include "pico_socket.h"
#include "pico_queue.h"

void print_forwarding_stats(void);

extern struct pico_protocol pico_proto_tcp;
int input_segment_compare(void *ka, void *kb);

PACKED_STRUCT_DEF pico_tcp_hdr {
    struct pico_trans trans;
    uint32_t seq;
    uint32_t ack;
    uint8_t len;
    uint8_t flags;
    uint16_t rwnd;
    uint16_t crc;
    uint16_t urgent;
};

PACKED_STRUCT_DEF tcp_pseudo_hdr_ipv4
{
    struct pico_ip4 src;
    struct pico_ip4 dst;
    uint16_t tcp_len;
    uint8_t res;
    uint8_t proto;
};

#define PICO_TCPHDR_SIZE 20
#define PICO_SIZE_TCPOPT_SYN 20
#define PICO_SIZE_TCPHDR (uint32_t)(sizeof(struct pico_tcp_hdr))
#define PICO_TCP_IW          2

/* TCP options */
#define PICO_TCP_OPTION_END         0x00
#define PICO_TCPOPTLEN_END        1u
#define PICO_TCP_OPTION_NOOP        0x01
#define PICO_TCPOPTLEN_NOOP       1
#define PICO_TCP_OPTION_MSS         0x02
#define PICO_TCPOPTLEN_MSS        4
#define PICO_TCP_OPTION_WS          0x03
#define PICO_TCPOPTLEN_WS         3u
#define PICO_TCP_OPTION_SACK_OK        0x04
#define PICO_TCPOPTLEN_SACK_OK       2
#define PICO_TCP_OPTION_SACK        0x05
#define PICO_TCPOPTLEN_SACK       2 /* Plus the block */
#define PICO_TCP_OPTION_TIMESTAMP   0x08
#define PICO_TCPOPTLEN_TIMESTAMP  10u

/* TCP flags */
#define PICO_TCP_FIN 0x01u
#define PICO_TCP_SYN 0x02u
#define PICO_TCP_RST 0x04u
#define PICO_TCP_PSH 0x08u
#define PICO_TCP_ACK 0x10u
#define PICO_TCP_URG 0x20u
#define PICO_TCP_ECN 0x40u
#define PICO_TCP_CWR 0x80u

#define PICO_TCP_SYNACK    (PICO_TCP_SYN | PICO_TCP_ACK)
#define PICO_TCP_PSHACK    (PICO_TCP_PSH | PICO_TCP_ACK)
#define PICO_TCP_FINACK    (PICO_TCP_FIN | PICO_TCP_ACK)
#define PICO_TCP_FINPSHACK (PICO_TCP_FIN | PICO_TCP_PSH | PICO_TCP_ACK)
#define PICO_TCP_RSTACK    (PICO_TCP_RST | PICO_TCP_ACK)

/* moved from tcp.c */
#define TCP_IS_STATE(s, st) ((s->state & PICO_SOCKET_STATE_TCP) == st)
#define TCP_SOCK(s) ((struct pico_socket_tcp *)s)
#define SEQN(f) ((f) ? (long_be(((struct pico_tcp_hdr *)((f)->transport_hdr))->seq)) : 0)
#define ACKN(f) ((f) ? (long_be(((struct pico_tcp_hdr *)((f)->transport_hdr))->ack)) : 0)

#define TCP_TIME (pico_time)(PICO_TIME_MS())

#define PICO_TCP_RTO_MIN (70)
#define PICO_TCP_RTO_MAX (120000)
#define PICO_TCP_IW          2
#define PICO_TCP_SYN_TO  2000u
#define PICO_TCP_ZOMBIE_TO 30000

#define PICO_TCP_MAX_RETRANS         10
#define PICO_TCP_MAX_CONNECT_RETRIES 3

#define PICO_TCP_LOOKAHEAD      0x00
#define PICO_TCP_FIRST_DUPACK   0x01
#define PICO_TCP_SECOND_DUPACK  0x02
#define PICO_TCP_RECOVER        0x03
#define PICO_TCP_BLACKOUT       0x04
#define PICO_TCP_UNREACHABLE    0x05
#define PICO_TCP_WINDOW_FULL    0x06


struct pico_tcp_queue
{
    struct pico_tree pool;
    uint32_t max_size;
    uint32_t size;
    uint32_t frames;
};

/* Queues */
static struct pico_queue UNUSED tcp_in = {
    0
};
static struct pico_queue UNUSED tcp_out = {
    0
};

/* Structure for TCP socket */
struct tcp_sack_block {
    uint32_t left;
    uint32_t right;
    struct tcp_sack_block *next;
};

struct pico_socket_tcp_dump {
    struct pico_socket_dump sock_dump;

    uint32_t snd_nxt;
    uint32_t snd_last;
    uint32_t snd_old_ack;
    uint32_t snd_retry;
    uint32_t snd_last_out;

    /* congestion control */
    uint32_t avg_rtt;
    uint32_t rttvar;
    uint32_t rto;
    uint32_t in_flight;
    uint32_t retrans_tmr;
    pico_time retrans_tmr_due;
    uint16_t cwnd_counter;
    uint16_t cwnd;
    uint16_t ssthresh;
    uint16_t recv_wnd;
    uint16_t recv_wnd_scale;

    /* tcp_input */
    uint32_t rcv_nxt;
    uint32_t rcv_ackd;
    uint32_t rcv_processed;
    uint16_t wnd;
    uint16_t wnd_scale;
    uint16_t remote_closed;

    /* options */
    uint32_t ts_nxt;
    uint16_t mss;
    uint8_t sack_ok;
    uint8_t ts_ok;
    uint8_t mss_ok;
    uint8_t scale_ok;
    struct tcp_sack_block *sacks;
    uint8_t jumbo;
    uint32_t linger_timeout;

    /* Transmission */
    uint8_t x_mode;
    uint8_t dupacks;
    uint8_t backoff;
    uint8_t localZeroWindow;

    /* Keepalive */
    uint32_t keepalive_tmr;
    pico_time ack_timestamp;
    uint32_t ka_time;
    uint32_t ka_intvl;
    uint32_t ka_probes;
    uint32_t ka_retries_count;

    /* FIN timer */
    uint32_t fin_tmr;
};

struct pico_socket_tcp {
    struct pico_socket sock;

    /* Tree/queues */
    struct pico_tcp_queue tcpq_in;  /* updated the input queue to hold input segments not the full frame. */
    struct pico_tcp_queue tcpq_out;
    struct pico_tcp_queue tcpq_hold; /* buffer to hold delayed frames according to Nagle */

    /* tcp_output */
    uint32_t snd_nxt;
    uint32_t snd_last;
    uint32_t snd_old_ack;
    uint32_t snd_retry;
    uint32_t snd_last_out;

    /* congestion control */
    uint32_t avg_rtt;
    uint32_t rttvar;
    uint32_t rto;
    uint32_t in_flight;
    uint32_t retrans_tmr;
    pico_time retrans_tmr_due;
    uint16_t cwnd_counter;
    uint16_t cwnd;
    uint16_t ssthresh;
    uint16_t recv_wnd;
    uint16_t recv_wnd_scale;

    /* tcp_input */
    uint32_t rcv_nxt;
    uint32_t rcv_ackd;
    uint32_t rcv_processed;
    uint16_t wnd;
    uint16_t wnd_scale;
    uint16_t remote_closed;

    /* options */
    uint32_t ts_nxt;
    uint16_t mss;
    uint8_t sack_ok;
    uint8_t ts_ok;
    uint8_t mss_ok;
    uint8_t scale_ok;
    struct tcp_sack_block *sacks;
    uint8_t jumbo;
    uint32_t linger_timeout;

    /* Transmission */
    uint8_t x_mode;
    uint8_t dupacks;
    uint8_t backoff;
    uint8_t localZeroWindow;

    /* Keepalive */
    uint32_t keepalive_tmr;
    pico_time ack_timestamp;
    uint32_t ka_time;
    uint32_t ka_intvl;
    uint32_t ka_probes;
    uint32_t ka_retries_count;

    /* FIN timer */
    uint32_t fin_tmr;
};


PACKED_STRUCT_DEF pico_tcp_option
{
    uint8_t kind;
    uint8_t len;
};

struct pico_socket *pico_tcp_open(uint16_t family);
struct pico_socket *hs_pico_tcp_open(uint16_t family, void *timer_heap);
uint32_t pico_tcp_read(struct pico_socket *s, void *buf, uint32_t len);
int pico_tcp_initconn(struct pico_socket *s);
int pico_tcp_input(struct pico_socket *s, struct pico_frame *f);
uint16_t pico_tcp_checksum(struct pico_frame *f);
uint16_t pico_tcp_checksum_ipv4(struct pico_frame *f);
#ifdef PICO_SUPPORT_IPV6
uint16_t pico_tcp_checksum_ipv6(struct pico_frame *f);
#endif
uint16_t pico_tcp_overhead(struct pico_socket *s);
int pico_tcp_output(struct pico_socket *s, int loop_score);
int pico_tcp_queue_in_is_empty(struct pico_socket *s);
int pico_tcp_reply_rst(struct pico_frame *f);
void pico_tcp_cleanup_queues(struct pico_socket *sck);
void pico_tcp_notify_closing(struct pico_socket *sck);
void pico_tcp_flags_update(struct pico_frame *f, struct pico_socket *s);
int pico_tcp_set_bufsize_in(struct pico_socket *s, uint32_t value);
int pico_tcp_set_bufsize_out(struct pico_socket *s, uint32_t value);
int pico_tcp_get_bufsize_in(struct pico_socket *s, uint32_t *value);
int pico_tcp_get_bufsize_out(struct pico_socket *s, uint32_t *value);
int pico_tcp_set_keepalive_probes(struct pico_socket *s, uint32_t value);
int pico_tcp_set_keepalive_intvl(struct pico_socket *s, uint32_t value);
int pico_tcp_set_keepalive_time(struct pico_socket *s, uint32_t value);
int pico_tcp_set_linger(struct pico_socket *s, uint32_t value);
uint16_t pico_tcp_get_socket_mss(struct pico_socket *s);
int pico_tcp_check_listen_close(struct pico_socket *s);
void tcp_parse_options(struct pico_frame *f);
uint32_t pico_paws(void);
void rto_set(struct pico_socket_tcp *t, uint32_t rto);
int tcp_send_synack(struct pico_socket *s);
void tcp_set_init_point(struct pico_socket *s);
int tcp_send_rst(struct pico_socket *s, struct pico_frame *fr);
int tcp_ack(struct pico_socket *s, struct pico_frame *f);
int hs_tcp_ack(struct pico_socket *s, struct pico_frame *f, void *timers);
void tcp_set_space(struct pico_socket_tcp *t);
void tcp_add_options(struct pico_socket_tcp *ts, struct pico_frame *f, uint16_t flags, uint16_t optsiz);
uint16_t tcp_options_size(struct pico_socket_tcp *t, uint16_t flags);
void tcp_send_ack(struct pico_socket_tcp *t);

#endif
