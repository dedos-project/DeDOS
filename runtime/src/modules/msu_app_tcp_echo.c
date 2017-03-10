#define _GNU_SOURCE
#include <string.h>
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

//#include "utils.h"

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

#include "pico_socket.h"
#include "pico_queue.h"
#include "pico_frame.h"
#include "runtime.h"
#include "modules/msu_app_tcp_echo.h"
#include "pico_socket_tcp.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_list.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"

#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>

void msu_app_tcpecho(uint16_t listen_port);
void msu_cb_tcpecho(uint16_t ev, struct pico_socket *s);
int msu_send_tcpecho(struct pico_socket *s);

#define BSIZE (1024 * 10)

static char recvbuf[BSIZE];
static int pos = 0, len = 0;
static int flag = 0;

static char *msu_cpy_arg(char **dst, char *str)
{
    char *p, *nxt = NULL;
    char *start = str;
    char *end = start + strlen(start);
    char sep = ':';

    p = str;
    while (p) {
        if ((*p == sep) || (*p == '\0')) {
            *p = (char)0;
            nxt = p + 1;
            if ((*nxt == 0) || (nxt >= end))
                nxt = 0;

            log_debug("dup'ing %s\n", start);
            *dst = strdup(start);
            break;
        }

        p++;
    }
    return nxt;
}

int msu_send_tcpecho(struct pico_socket *s)
{
    int w, ww = 0;
    if (len > pos) {
        do {
            w = pico_socket_write(s, recvbuf + pos, len - pos);
            if (w > 0) {
                pos += w;
                ww += w;
                if (pos >= len) {
                    pos = 0;
                    len = 0;
                }
            }
        } while((w > 0) && (pos < len));
    }

    return ww;
}

void msu_cb_tcpecho(uint16_t ev, struct pico_socket *s)
{
    int r = 0;

    // printf("tcpecho> wakeup ev=%u\n", ev);

    if (ev & PICO_SOCK_EV_RD) {
        if (flag & PICO_SOCK_EV_CLOSE){
            ;
        }
            // printf("SOCKET> EV_RD, FIN RECEIVED\n");

        while (len < BSIZE) {
            r = pico_socket_read(s, recvbuf + len, BSIZE - len);
            if (r > 0) {
                len += r;
                flag &= ~(PICO_SOCK_EV_RD);
            } else {
                flag |= PICO_SOCK_EV_RD;
                break;
            }
        }
        if (flag & PICO_SOCK_EV_WR) {
            flag &= ~PICO_SOCK_EV_WR;
            msu_send_tcpecho(s);
        }
    }

    if (ev & PICO_SOCK_EV_CONN) {
        uint32_t ka_val = 0;
        struct pico_socket *sock_a = {
            0
        };
        struct pico_ip4 orig = {
            0
        };
        uint16_t port = 0;
        char peer[30] = {
            0
        };
        int yes = 1;

        sock_a = pico_socket_accept(s, &orig, &port);
        pico_ipv4_to_string(peer, orig.addr);
        log_debug("Connection established with %s:%d", peer, short_be(port));
        pico_socket_setoption(sock_a, PICO_TCP_NODELAY, &yes);
        /* Set keepalive options */
        ka_val = 5;
        pico_socket_setoption(sock_a, PICO_SOCKET_OPT_KEEPCNT, &ka_val);
        ka_val = 30000;
        pico_socket_setoption(sock_a, PICO_SOCKET_OPT_KEEPIDLE, &ka_val);
        ka_val = 5000;
        pico_socket_setoption(sock_a, PICO_SOCKET_OPT_KEEPINTVL, &ka_val);
    }

    if (ev & PICO_SOCK_EV_FIN) {
        ;
        // printf("Socket closed. Exit normally. \n");
        //pico_timer_add(2000, deferred_exit, NULL);
    }

    if (ev & PICO_SOCK_EV_ERR) {
        // printf("Socket error received: %s. Bailing out.\n", strerror(pico_err));
        //exit(1);
        ;
    }

    if (ev & PICO_SOCK_EV_CLOSE) {
        // printf("Socket received close from peer.\n");
        if (flag & PICO_SOCK_EV_RD) {
            pico_socket_shutdown(s, PICO_SHUT_WR);
            // printf("SOCKET> Called shutdown write, ev = %d\n", ev);
        }
    }

    if (ev & PICO_SOCK_EV_WR) {
        r = msu_send_tcpecho(s);
        if (r == 0)
            flag |= PICO_SOCK_EV_WR;
        else
            flag &= (~PICO_SOCK_EV_WR);
    }
}

void msu_app_tcpecho(uint16_t listen_port)
{

    int ret = 0, yes = 1;
    struct pico_socket *s = NULL;
    union {
        struct pico_ip4 ip4;
        struct pico_ip6 ip6;
    } inaddr_any = {
        .ip4 = {0}, .ip6 = {{0}}
    };

    s = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_TCP, &msu_cb_tcpecho);

    if (!s) {
        log_error("opening socket: %s", strerror(pico_err));
        exit(1);
    }

    pico_socket_setoption(s, PICO_TCP_NODELAY, &yes);
    listen_port = short_be(listen_port);
    ret = pico_socket_bind(s, &inaddr_any.ip4, &listen_port);

    if (ret < 0) {
        printf("%s: error binding socket to port %u: %s\n", __FUNCTION__, short_be(listen_port), strerror(pico_err));
        exit(1);
    }

    if (pico_socket_listen(s, 10000) != 0) {
        printf("%s: error listening on port %u\n", __FUNCTION__, short_be(listen_port));
        exit(1);
    }

    log_info("Launching PicoTCP echo server on port: %d\n",short_be(listen_port));
    return;

out:
    fprintf(stderr, "tcpecho expects the following format: tcpecho:listen_port\n");
    exit(255);
}

int msu_app_tcp_echo_process_queue_item(struct generic_msu *msu, struct generic_msu_queue_item *queue_item){
    /* function called when an item is dequeued */
    /* queue_item can be parsed into the struct with is expected in the queue */

    //should free the queue item here
    //But we will just enqueue it again to keep the queue filled for this function to be called

    return 0;
}
int msu_app_tcp_echo_init(local_msu *self, 
        struct create_msu_thread_msg_data *create_action)
{
    struct pico_ip4 ZERO_IP4 = {
        0
    };
    struct pico_ip4 bcastAddr = ZERO_IP4;

    unsigned char macaddr[6] = { 0, 0, 0, 0xa, 0xb, 0x0 };
    uint16_t *macaddr_low = (uint16_t *) (macaddr + 2);
    struct pico_device *dev = NULL;
    struct pico_ip4 addr4 = { 0 };

    *macaddr_low = (uint16_t) (*macaddr_low
            ^ (uint16_t) ((uint16_t) getpid() & (uint16_t) 0xFFFFU));
    log_debug("My macaddr base is: %02x %02x\n", macaddr[2], macaddr[3]);
    log_debug("My macaddr is: %02x %02x %02x %02x %02x %02x\n", macaddr[0],
            macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

    char *nxt, *name = NULL, *addr = NULL, *nm = NULL, *gw = NULL, *lport;
    struct pico_ip4 ipaddr, netmask, gateway, zero = ZERO_IP4;
    int listen_port;
    char *optarg = (char*)malloc(sizeof(char) * create_action->init_data_len + 1);
    strncpy(optarg, create_action->creation_init_data,create_action->init_data_len);
    optarg[create_action->init_data_len] = '\0';

    do {
        nxt = msu_cpy_arg(&name, optarg);
        if (!nxt)
            break;

        nxt = msu_cpy_arg(&addr, nxt);
        if (!nxt)
            break;

        nxt = msu_cpy_arg(&nm, nxt);
        if (!nxt)
            break;

        msu_cpy_arg(&lport, nxt);

    } while (0);

    if (!nm) {
        log_error("Bad tap configuration...%s","");
        return -1;
    }

    dev = pico_tap_create(name);
    if (!dev) {
        log_error("Failed to create tap device%s","");
        return -1;
    }
    log_debug("Dev created: name: %s",name);

    pico_string_to_ipv4(addr, &ipaddr.addr);
    pico_string_to_ipv4(nm, &netmask.addr);
    pico_ipv4_link_add(dev, ipaddr, netmask);
    bcastAddr.addr = (ipaddr.addr) | (~netmask.addr);

    listen_port = atoi(lport);
    log_debug("tcpecho listen port: %d",listen_port);
    msu_app_tcpecho(listen_port);

    free(optarg);

    log_debug("Created %s MSU with id: %u", self->type->name,
            self->id);
    /* for sake of running this msu since no msu writes to this MSU's input queue,
     * we will write some dummy data to its own queue to be dequeued when thread loop
     * iterates over all MSU to check it they should run it.
     */
    // struct generic_msu_queue_item *dummy_item;
    // dummy_item = malloc(sizeof(struct generic_msu_queue_item));
    // generic_msu_queue_enqueue(tcp_echo_msu->q_in, dummy_item);
    // log_debug("Enqueued an empty frame to keep the thread loop happy and get a call to data handler %s","");

    return 0;
}

msu_type MSU_APP_TCP_ECHO_TYPE = {
    .name="app_tcp_echo_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_PICO_TCP_APP_TCP_ECHO_ID,
    .proto_number=MSU_PROTO_PICO_TCP_APP_ECHO,
    .init=msu_app_tcp_echo_init,
    .destroy=NULL,
    .receive=msu_app_tcp_echo_process_queue_item,
    .receive_ctrl=NULL,
    .route=round_robin,
    .deserialize=default_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote
};



