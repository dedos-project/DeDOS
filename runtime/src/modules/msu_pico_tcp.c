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
#include "pico_socket_tcp.h"

#include "dedos_msu_list.h"
#include "pico_queue.h"
#include "pico_frame.h"
#include "runtime.h"
#include "modules/msu_pico_tcp.h"
#include "modules/msu_app_tcp_echo.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"

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

int msu_pico_tcp_recv_state(struct generic_msu *self, struct dedos_intermsu_message* msg, void *buf, uint16_t bufsize){
    //copy the buf and enqueue the message into my queue
    //malloc intermsu_msg
    struct dedos_intermsu_message *intermsu_msg = (struct dedos_intermsu_message*)malloc(sizeof(struct dedos_intermsu_message));
    if(!intermsu_msg){
        log_error("Failed to malloc intermsu_msg in recv state%s","");
        return -1;
    }
    intermsu_msg->src_msu_id = msg->src_msu_id;
    intermsu_msg->dst_msu_id = msg->dst_msu_id;
    intermsu_msg->proto_msg_type = msg->proto_msg_type;
    intermsu_msg->payload_len = msg->payload_len;
    intermsu_msg->payload = (char*)malloc(bufsize);
    if(!intermsu_msg->payload){
        log_error("Failed to malloc intermsu_msg payload in recv state%s","");
        free(intermsu_msg);
        return -1;
    }
    memcpy(intermsu_msg->payload, buf, bufsize);

    //create a queue item
    struct generic_msu_queue_item *queue_item = (struct generic_msu_queue_item*)malloc(sizeof(struct generic_msu_queue_item));
    if(!queue_item){
        log_error("Failed to malloc queue item in recv state %s","");
        free(intermsu_msg->payload);
        free(intermsu_msg);
        return -1;
    }
    queue_item->buffer = intermsu_msg;
    queue_item->buffer_len = sizeof(struct dedos_intermsu_message) + bufsize;
    int ret = generic_msu_queue_enqueue(&self->q_in, queue_item);
    if(ret < 0){
        log_error("Failed to enqueue recv state in to own queue for processing%s","");
        free(queue_item);
        free(intermsu_msg->payload);
        free(intermsu_msg);
        return -1;
    }
    return 0;
}

int msu_pico_tcp_restore(struct generic_msu *self,
        struct dedos_intermsu_message* msg, void *buf, uint16_t bufsize)
{

    if (msg->proto_msg_type == MSU_PROTO_TCP_HANDSHAKE_RESPONSE) {
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
        int ret = 0;
        uint8_t flags = hdr->flags;
        struct pico_ipv4_hdr *iphdr = (struct pico_ipv4_hdr *)f->net_hdr;

        if (flags == (PICO_TCP_SYN | PICO_TCP_ACK)){
            log_debug("%s","SYNACK from HS, pushing out to client");

            f->len = f->len - PICO_SIZE_ETHHDR;
            f->start = f->net_hdr;
//            pico_frame_to_buf(f);

            struct pico_device *dev = pico_ipv4_source_dev_find(&iphdr->dst.addr);
            f->dev = dev;
            log_debug("Dev name being used: %s",f->dev->name);
            if(!f->dev){
                log_error("%s","No device found to forward SYNACK from HS to client");
                return -1;
            }
            log_debug("sending frame to dev %s","");
            print_frame(f);
            pico_sendto_dev(f);
            log_debug("%s","Enqueued SYNACK for sending to client");
        }
        else {
            log_warn("Should never be here...ever...%s","");
        }
    } else if (msg->proto_msg_type == MSU_PROTO_TCP_CONN_RESTORE) { //socket to restore
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
            return -1;
        }
        // log_debug("After getting sockport %s","");
        struct pico_socket *listen_sock = find_listen_sock(data_sp, s);
        if (!listen_sock) {
            pico_socket_close(s);
            log_error("ERROR: No listen sock in orig %s", "");
        }
        log_debug("Found listen sock %s","");

        t->sock.parent = listen_sock;
        log_debug("After assigning listen sock to t->sock.parent %s","");
        log_debug("Listen sock: %d",short_be(listen_sock->local_port));
        t->sock.wakeup = listen_sock->wakeup;

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
    return 0;
}
int msu_pico_tcp_process_queue_item(struct generic_msu *msu, msu_queue_item *queue_item){
    /* function called when an item is dequeued */
    /* queue_item can be parsed into the struct with is expected in the queue */
#ifdef PICO_SUPPORT_TIMINGS
    START_TIME;
#endif

    pico_stack_tick();

#ifdef PICO_SUPPORT_TIMINGS
    END_TIME("dedos_pico_stack_tick");
#endif

    //case when queue_item is NULL to keep the stack ticking
    if(!queue_item){
    	return 0;
    }
    //queue items playload should be an intermsu msg
    //intermsu's msg payload should be a pico frame
    struct dedos_intermsu_message *msg;
    log_debug("Dequeued an item in msu id: %d",msu->id);

    msg = (struct dedos_intermsu_message*)queue_item->buffer;
    msu_pico_tcp_restore(msu, msg, msg->payload, msg->payload_len);

    log_debug("Processed queue item of %s",msu->type->name);

    return -10;
}


int msu_pico_tcp_init(struct generic_msu *self, 
        struct create_msu_thread_msg_data *create_action)
{
    // pico_tcp_msu->restore = msu_pico_tcp_restore; //called when data is received over control socket i.e. TCP state from HS MSU

    /* for sake of running this msu since no msu writes to this MSU's input queue,
     * we will write some dummy data to its own queue to be dequeued when thread loop
     * iterates over all MSU to check it they should run it.
     */

    log_info("Initializing pico_tcp_stack by calling pico_stack_init %s","");
    pico_tcp_msu = self;
    pico_stack_init();
    // Following is for pico_tcp busy loop
    int rtn = sem_post(self->q_in.thread_q_sem);
    if (rtn < 0){
        log_error("error incrementing thread queue semaphore");
    }
//
//    struct generic_msu_queue_item *dummy_item;
//    dummy_item = malloc(sizeof(struct generic_msu_queue_item));
//    dummy_item->buffer = NULL;
//    dummy_item->buffer_len = 0;
//    generic_msu_queue_enqueue(pico_tcp_msu->q_in, dummy_item);
//    debug("DEBUG: Enqueued an empty frame to keep the thread loop happy and get a call to data handler %s","");

    return 0;
}

struct msu_type PICO_TCP_MSU_TYPE= {
    .name="pico_tcp_msu",
    .layer=DEDOS_LAYER_ALL,
    .type_id=DEDOS_PICO_TCP_STACK_MSU_ID | DEDOS_TCP_DATA_MSU_ID,
    .proto_number=MSU_PROTO_PICO_TCP_STACK,
    .init=msu_pico_tcp_init,
    .destroy=NULL,
    .receive=msu_pico_tcp_process_queue_item,
    .receive_ctrl=NULL,
    .route=round_robin,
    .deserialize=msu_pico_tcp_recv_state,
    .send_local=default_send_local,
    .send_remote=default_send_remote
};
