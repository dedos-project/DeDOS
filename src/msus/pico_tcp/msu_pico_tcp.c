#define _GNU_SOURCE
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "pico_stack.h"
#include "pico_config.h"
#include "pico_dev_vde.h"
#include "pico_ipv4.h"
#include "pico_ipv6.h"
#include "pico_socket.h"
#include "pico_dev_tun.h"
#include "pico_dev_tap.h"
#include "pico_nat.h"
#include "pico_icmp4.h"
#include "pico_icmp6.h"
#include "pico_dns_client.h"
#include "pico_dev_loop.h"
#include "pico_dhcp_client.h"
#include "pico_dhcp_server.h"
#include "pico_ipfilter.h"
#include "pico_olsr.h"
#include "pico_sntp_client.h"
#include "pico_mdns.h"
#include "pico_tftp.h"
#include "pico_tcp.h"
#include "pico_eth.h"
#include "pico_dev_pcap.h"
#include "pico_socket_tcp.h"

#include "pico_queue.h"
#include "pico_frame.h"
#include "pico_tcp/msu_pico_tcp.h"
#include "pico_tcp/msu_app_tcp_echo.h"
#include "communication.h"
#include "routing.h"
#include "logging.h"
#include "msu_message.h"
#include "local_msu.h"
#include "uthash.h"
#include "msu_calls.h"

static struct local_msu *PICO_TCP_MSU;

extern long unsigned int synacks_to_client;
extern long unsigned int sockets_restore_count;
extern long unsigned int forwarding_items_dequeued;

static struct pico_socket *match_tcp_sockets(struct pico_socket *s1,
        struct pico_socket *s2)
{
    struct pico_socket *found = NULL;
    if ((s1->remote_port == s2->remote_port) && /* remote port check */
    (s1->remote_addr.ip4.addr == s2->remote_addr.ip4.addr)
            && /* remote addr check */
            ((s1->local_addr.ip4.addr == PICO_IPV4_INADDR_ANY)
                    || (s1->local_addr.ip4.addr == s2->local_addr.ip4.addr))) { /* Either local socket is ANY, or matches dst */
        found = s1;
        return found;
    } else if ((s1->remote_port == 0) && /* not connected... listening */
    (s1->local_port == s2->local_port)) { /* Either local socket is ANY, or matches dst */
        /* listen socket */
        found = s1;
    }

    return found;
}

static struct pico_socket *find_listen_sock(struct pico_sockport *sp, struct pico_socket *sock)
{

    struct pico_socket *found = NULL;
    struct pico_tree_node *index = NULL;
    struct pico_tree_node *_tmp;
    struct pico_socket *s = NULL;

    pico_tree_foreach_safe(index, &sp->socks, _tmp)
    {
        s = index->keyValue;
        /* 4-tuple identification of socket (port-IP) */
        if (sock->net->proto_number == PICO_PROTO_IPV4) {
            found = match_tcp_sockets(s, sock);
        }

        if (found)
            break;
    } /* FOREACH */

    return found;
}

int msu_pico_tcp_restore(unsigned int proto_msg_type, void *buf, uint16_t bufsize)
{
    //forwarding_items_dequeued++;
    if (proto_msg_type == MSU_PROTO_TCP_HANDSHAKE_RESPONSE) {
        log_debug("%s", "Received a valid TCP frame as queue item");
        struct pico_frame *f = pico_frame_alloc(bufsize);
        char *bufs = (char *) buf;

        memcpy(f->buffer, bufs, bufsize);
        f->buffer_len = bufsize;
        f->start = f->buffer;
        f->len = f->buffer_len;
        f->datalink_hdr = f->buffer;
        f->net_hdr = f->datalink_hdr + PICO_SIZE_ETHHDR;
        f->net_len = 20;
        f->transport_hdr = f->net_hdr + f->net_len;
        f->transport_len = (uint16_t) (f->len - f->net_len
                - (uint16_t) (f->net_hdr - f->buffer));

        // print_frame(f);

        struct pico_tcp_hdr *hdr = (struct pico_tcp_hdr *) (f->transport_hdr);
        uint8_t flags = hdr->flags;
        struct pico_ipv4_hdr *iphdr = (struct pico_ipv4_hdr *)f->net_hdr;

        if (flags == (PICO_TCP_SYN | PICO_TCP_ACK) || PICO_TCP_RST){
            log_debug("%s","Stuff from HS, pushing out to client");

            f->len = f->len - PICO_SIZE_ETHHDR;
            f->start = f->net_hdr;
//            pico_frame_to_buf(f);

            struct pico_device *dev = pico_ipv4_source_dev_find((struct pico_ip4*)&iphdr->dst.addr);
            f->dev = dev;
/*
            if(!f->dev){
                log_error("%s","No device found to forward SYNACK from HS to client");
                return -1;
            }
            log_debug("Dev name being used: %s",f->dev->name);
*/
            //print_frame(f);
            int32_t stat = pico_sendto_dev(f);
            if(stat > -1 && flags == (PICO_TCP_SYN | PICO_TCP_ACK)) {
                synacks_to_client++;
            }
            log_debug("%s","Enqueued SYNACK for sending to client");
        }
        else {
            log_critical("Should never be here...ever...%0x",flags);
        }
    } else if (proto_msg_type == MSU_PROTO_TCP_CONN_RESTORE) { //socket to restore
        //Part of restore code never executed on HS
        log_debug("%s", "Got a connection restore msg");
        struct pico_socket_tcp_dump *t_dump = (struct pico_socket_tcp_dump *) buf;
        struct pico_socket_dump *s_dump = (struct pico_socket_dump*) t_dump;

        struct pico_socket *s = pico_socket_tcp_open(PICO_PROTO_IPV4);
        struct pico_socket_tcp *t = (struct pico_socket_tcp *) s;
        uint16_t mtu;

        if (s_dump->l4_proto == PICO_PROTO_TCP) {
            s->proto = &pico_proto_tcp;
        }
        if (s_dump->l3_net == PICO_PROTO_IPV4) {
            s->net = &pico_proto_ipv4;
        }
        log_debug("Assigned net and proto %s","");
        mtu = (uint16_t) pico_socket_get_mss(&t->sock);

        s->q_in.max_size = PICO_DEFAULT_SOCKETQ;
        s->q_out.max_size = PICO_DEFAULT_SOCKETQ;
        s->wakeup = NULL;

        t->tcpq_in.max_size = PICO_DEFAULT_TCP_SOCKETQ;
        t->tcpq_out.max_size = PICO_DEFAULT_TCP_SOCKETQ;
        t->tcpq_hold.max_size = 2u * mtu;
        memcpy(&s->local_addr, &s_dump->local_addr, sizeof(struct pico_ip4));
        memcpy(&s->remote_addr, &s_dump->remote_addr, sizeof(struct pico_ip4));

        s->local_port = s_dump->local_port;
        s->remote_port = s_dump->remote_port;

        log_debug("Assigned ports and addrs %s","");

        s->ev_pending = s_dump->ev_pending;
        s->max_backlog = s_dump->max_backlog;
        s->number_of_pending_conn = s_dump->number_of_pending_conn;

        s->state = s_dump->state;
        s->opt_flags = s_dump->opt_flags;
        s->timestamp = s_dump->timestamp;

        // debug("DEBUG : Before memcpy of socket data %s","");

        //copy tcp socket data
        memcpy(&t->snd_nxt, &t_dump->snd_nxt,
                sizeof(struct pico_socket_tcp_dump)
                        - sizeof(struct pico_socket_dump));
        //Add restored socket to global tree
        // debug("DEBUG : After memcpy of socket data %s","");

        struct pico_sockport *data_sp = pico_get_sockport(PICO_PROTO_TCP, s->local_port);
        if (!data_sp) {
            log_error("No such port %u", short_be(s->local_port));
            pico_socket_close(s);
            goto end;
        }
        // log_debug("After getting sockport %s","");
        struct pico_socket *listen_sock = find_listen_sock(data_sp, s);
        if (!listen_sock) {
            pico_socket_close(s);
            log_error("ERROR: No listen sock in orig %s", "");
            goto end;
        }
        log_debug("Found listen sock %s","");

        t->sock.parent = listen_sock;
        log_debug("After assigning listen sock to t->sock.parent %s","");
        log_debug("Listen sock: %d",short_be(listen_sock->local_port));
        t->sock.wakeup = listen_sock->wakeup;
        sockets_restore_count++;
        // debug("DEBUG : Before pico_socket_add %s","");
        pico_socket_add(&t->sock);
        log_debug("Added restored sock to sock_tree %s","");
        //msu_dbg("t->retrans_tmr : %u, retrans_tmr_due: %lu", t->retrans_tmr, t->retrans_tmr_due);
        t->retrans_tmr = 0;
        //msu_dbg("t->retrans_tmr : %u, retrans_tmr_due: %lu", t->retrans_tmr, t->retrans_tmr_due);
        log_debug("After restore: snd_nxt is now %08x", t->snd_nxt);
        log_debug("After restore: rcv_nxt is now %08x", t->rcv_nxt);

        tcp_send_ack(t);

        if (s->parent && s->parent->wakeup) {
            log_debug("FIRST ACK - Parent found -> listening socket %u",
                    short_be(s->local_port));
            s->parent->wakeup(PICO_SOCK_EV_CONN, s->parent);
        }
        s->ev_pending |= PICO_SOCK_EV_WR;
    }
    else{
        printf("Unknown msg type to restore......\n");
    }
end:
    free(buf);
    return 0;
}

int msu_pico_tcp_process_queue_item(struct local_msu *self, struct msu_msg *msg){
    /* function called when an item is dequeued */
    /* queue_item can be parsed into the struct with is expected in the queue */

    //FIXME: Assuming that there will only be 1 picotcp msu per runtime
    //else move the static function var to internal state
    static int covered_count;

    /*
    msu_queue *q = &msu->q_in;
    int rtn = sem_post(q->thread_q_sem);
    if (rtn < 0){
        log_error("error incrementing thread queue semaphore");
    }
    */

    //case when queue_item is NULL to keep the stack ticking
    if(msg->data){
        //queue items playload should be an intermsu msg
        //intermsu's msg payload should be a pico frame

        struct pico_tcp_msg *pico_msg = msg->data;

        log_debug("Dequeued an item in msu id: %d",self->id);

        unsigned int proto_msg_type = pico_msg->proto_msg_type;
        msu_pico_tcp_restore(proto_msg_type, pico_msg->payload, pico_msg->payload_len);

        log_debug("Processed queue item of %s",self->type->name);
#ifdef DATAPLANE_PROFILING
        if (proto_msg_type == MSU_PROTO_TCP_HANDSHAKE_RESPONSE) {
            log_dp_event(msu->id, DEDOS_EXIT, &queue_item->dp_profile_info);
        } else if (proto_msg_type == MSU_PROTO_TCP_CONN_RESTORE) { //socket to restore
            log_dp_event(msu->id, DEDOS_SINK, &queue_item->dp_profile_info);
        }
#endif
        free(pico_msg);
    }

    if(covered_count > (PICO_TCP_MSU->scheduling_weight * 2)){
        pico_stack_tick();
        covered_count = 0;
    }

    covered_count++;

    return -10;
}


int msu_pico_tcp_init(struct local_msu *self, struct msu_init_data *init_data) {
    // PICO_TCP_MSU->restore = msu_pico_tcp_restore; //called when data is received over control socket i.e. TCP state from HS MSU

    /* for sake of running this msu since no msu writes to this MSU's input queue,
     * we will write some dummy data to its own queue to be dequeued when thread loop
     * iterates over all MSU to check it they should run it.
     */

    if (PICO_TCP_MSU != NULL) {
        log_warn("Pico TCP MSU already instantiated!");
    }

    log_info("Initializing pico_tcp_stack by calling pico_stack_init %s","");
    PICO_TCP_MSU = self;
    int rtn = pico_stack_init();
    // TODO: self->scheduling_weight = 32;

    //printf("self ptr in init: %p\n", self);
    //printf("PICO_TCP_MSU: %p\n",PICO_TCP_MSU);
    //printf("sched weight; %d, %d\n", self->scheduling_weight, PICO_TCP_MSU->scheduling_weight);
    if (rtn < 0){
        log_error("error incrementing thread queue semaphore");
    }
    log_debug("post semaphore for tcp msu");
    struct msu_msg_key self_key = {};
    init_call_local_msu(self, self, &self_key, 0, NULL);
    return 0;
}

/**
 * Sets the ID of the incoming queue item to be a hash of the 4 tuple
 * @param self The picotcp MSU
 * @param queue_item the queue item on which the ID is to be set
 * @return queue item's ID
 */
int32_t pico_tcp_generate_id(struct local_msu *self, struct pico_frame *f){
    uint32_t id;
    int num_bytes;
    id = 0;
    num_bytes = 3 * sizeof(uint32_t);

    void *buf = malloc(num_bytes);
    if(!buf){
        log_error("failed to malloc buf for 4 tuple");
        return id;
    }
    struct pico_tcp_hdr *hdr = (struct pico_tcp_hdr *) (f->transport_hdr);
    struct pico_ipv4_hdr *net_hdr = (struct pico_ipv4_hdr *) f->net_hdr;
/*
    char peer[30], local[30];
    pico_ipv4_to_string(peer, net_hdr->src.addr);
    pico_ipv4_to_string(local, net_hdr->dst.addr);
    log_debug("Src addr %s: dst addr : %s", peer, local);
    log_debug("> dst port:%u src port: %u", short_be(hdr->trans.dport), short_be(hdr->trans.sport));
*/
    uint16_t sport = short_be(hdr->trans.sport);
    uint16_t dport = short_be(hdr->trans.dport);

    memcpy(buf, &(net_hdr->src.addr), sizeof(uint32_t));
    memcpy(buf + sizeof(uint32_t), &(net_hdr->dst.addr), sizeof(uint32_t));
    memcpy(buf + 2 * sizeof(uint32_t),&sport, sizeof(uint16_t));
    memcpy(buf + 5 * sizeof(uint16_t),&dport, sizeof(uint16_t));

    // Hash the 4 tuple into id
    HASH_VALUE(buf, num_bytes, id);
    free(buf);
    return id;
}

struct msu_type PICO_TCP_MSU_TYPE= {
    .name="pico_tcp_msu",
    .id=DEDOS_TCP_DATA_MSU_TYPE_ID,
    .init=msu_pico_tcp_init,
    .receive=msu_pico_tcp_process_queue_item,
};
