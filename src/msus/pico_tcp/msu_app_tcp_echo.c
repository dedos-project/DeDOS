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
#include "pico_dev_pcap.h"

#include "pico_socket.h"
#include "pico_queue.h"
#include "pico_frame.h"
#include "pico_tcp/msu_app_tcp_echo.h"
#include "pico_socket_tcp.h"
#include "communication.h"
#include "routing.h"
#include "pico_tcp/pico_msg_types.h"
#include "logging.h"
#include "local_msu.h"
#include "unused_def.h"

#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>

#define RECV_MODE 0
#define SEND_MODE 0
#define HANDSHAKE_ONLY_MODE 0

#define BSIZE (1024)
void msu_app_tcpecho(uint16_t listen_port);
void msu_cb_tcpecho(uint16_t ev, struct pico_socket *s);
int msu_send_tcpecho(struct pico_socket *s, char *recvbuf, int len);

union {
    struct pico_ip4 ip4;
    struct pico_ip6 ip6;
} inaddr_any = {
    .ip4 = {0}, .ip6 = {{0}}
};


static char UNUSED *msu_cpy_arg(char **dst, char *str)
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

int msu_send_tcpecho(struct pico_socket *s, char *recvbuf, int len)
{
#if RECV_MODE == 1
    //do not send back anything
    return 1;
#endif
    //log_warn("Recvd and sending:\n%s",recvbuf);
    int pos = 0;
    int w, ww = 0;
    char sendbuf[BSIZE];
    memset(sendbuf, '\0', sizeof(char)*BSIZE);
//    memcpy(&sendbuf, recvbuf, BSIZE);

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
    int len = 0;
    char recvbuf[BSIZE];
    memset(recvbuf, '\0', sizeof(char)*BSIZE);

    
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
        //pico_timer_add(2000, deferred_exit, NULL);
    }

    if (ev & PICO_SOCK_EV_ERR) {
        log_warn("Socket error received: %s. Bailing out.\n", strerror(pico_err));
        pico_socket_shutdown(s, PICO_SHUT_RDWR);
        //exit(1);
    }

    if (ev & PICO_SOCK_EV_CLOSE) {
        // printf("Socket received close from peer.\n");
        if (s->flag & PICO_SOCK_EV_RD) {
            pico_socket_shutdown(s, PICO_SHUT_WR);
        }
    }

    if (ev & PICO_SOCK_EV_WR) {
        r = 0;
        if(recvbuf[0] != '\0'){
            //r = msu_send_tcpecho(s, recvbuf, len);
        }
        if (r == 0)
            s->flag |= PICO_SOCK_EV_WR;
        else
            s->flag &= (~PICO_SOCK_EV_WR);
    }
    
    if (ev & PICO_SOCK_EV_RD) {
        if (s->flag & PICO_SOCK_EV_CLOSE){
            ;
        }

        memset(recvbuf, '\0', sizeof(char)*BSIZE);
        while (len < BSIZE) {
            r = pico_socket_read(s, recvbuf + len, BSIZE - len);
            if (r > 0) {
                len += r;
                s->flag &= ~(PICO_SOCK_EV_RD);
            } else {
                s->flag |= PICO_SOCK_EV_RD;
                break;
            }
        }
        if (s->flag & PICO_SOCK_EV_WR) {
            s->flag &= ~PICO_SOCK_EV_WR;
            msu_send_tcpecho(s, recvbuf, len);
        }
    }
}

void msu_app_tcpecho(uint16_t listen_port)
{

    int ret = 0, yes = 1;
    struct pico_socket *s = NULL;
    s = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_TCP, &msu_cb_tcpecho);

    if (!s) {
        log_error("opening socket: %s", strerror(pico_err));
        exit(1);
    }

    pico_socket_setoption(s, PICO_TCP_NODELAY, &yes);
    listen_port = short_be(listen_port);
    ret = pico_socket_bind(s, &inaddr_any.ip4, &listen_port);

    if (ret < 0) {
        log_error("error binding socket to port %u: %s\n",
                  short_be(listen_port), strerror(pico_err));
        exit(1);
    }

    if (pico_socket_listen(s, 10000) != 0) {
        printf("%s: error listening on port %u\n", __FUNCTION__, short_be(listen_port));
        exit(1);
    }

    log_info("Launching PicoTCP echo server on port: %d\n",short_be(listen_port));
    return;

}

static int msu_app_tcp_echo_receive(struct local_msu *self, struct msu_msg *data) {
    /* function called when an item is dequeued */
    /* queue_item can be parsed into the struct with is expected in the queue */

    //should free the queue item here
    //But we will just enqueue it again to keep the queue filled for this function to be called

    return 0;
}

static int msu_app_tcp_echo_init(struct local_msu *self, struct msu_init_data *init_data) {
    #define PCAP 1
    #define TAP 2
    //

//    char *nxt, *name = NULL, *addr = NULL, *nm = NULL, *gw = NULL, *lport;
    // 
    log(LOG_PICO_TCP_ECHO, "init_data: %s",init_data->init_data);
    char *name = "em4", *addr = "10.0.0.10", *nm = "255.255.255.0", *lport="6667";
    struct pico_ip4 ipaddr, netmask;
    int listen_port;
    char *optarg = init_data->init_data;
    int mode = 0;

    char *if_file_name = "em4";

/*
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
*/
    if(strcmp(name, "myTAP") == 0){
        mode = TAP;
    }
    else{
        mode = PCAP;
    }
    unsigned char macaddr[6] = {0}; 
    if(mode == PCAP){
        unsigned char macaddr_pcap[6] = { 0xf8, 0xbc, 0x12, 0x1a, 0xdf, 0xb1 };
        memcpy(macaddr, macaddr_pcap, sizeof(macaddr_pcap));
    }else{
        unsigned char macaddr_tap[6] = { 0, 0, 0, 0xa, 0xb, 0x0 };
        memcpy(macaddr, macaddr_tap, sizeof(macaddr_tap));
    }
    uint16_t *macaddr_low = (uint16_t *) (macaddr + 2);
    struct pico_device *dev = NULL;
    *macaddr_low = (uint16_t) (*macaddr_low
            ^ (uint16_t) ((uint16_t) getpid() & (uint16_t) 0xFFFFU));
    log(LOG_PICO_TCP_ECHO, "My macaddr base is: %02x %02x\n", macaddr[2], macaddr[3]);
    log(LOG_PICO_TCP_ECHO, "My macaddr is: %02x %02x %02x %02x %02x %02x\n", macaddr[0],
            macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);


    if (!nm) {
        log_error("bad configuration...%s","");
        return -1;
    }

    if(mode == PCAP){
        dev = pico_pcap_create_live(if_file_name, name, NULL);
        if (!dev) {
            log_error("Failed to create pcap device%s","");
            return -1;
        }
        log(LOG_PICO_TCP_ECHO, "Dev created: name: %s",name);
    }
    else {
        dev = pico_tap_create(name);
         if (!dev) {
            log_error("Failed to create pcap device%s","");
            return -1;
        }
        log(LOG_PICO_TCP_ECHO, "Dev created: name: %s",name);
    }
    pico_string_to_ipv4(addr, &ipaddr.addr);
    pico_string_to_ipv4(nm, &netmask.addr);
    pico_ipv4_link_add(dev, ipaddr, netmask);

    listen_port = atoi(lport);
    log(LOG_PICO_TCP_ECHO, "tcpecho listen port: %d",listen_port);
    msu_app_tcpecho(listen_port);

    free(optarg);

    log(LOG_PICO_TCP_ECHO, "Created %s MSU with id: %u", self->type->name,
            self->id);

    return 0;
}

struct msu_type MSU_APP_TCP_ECHO_TYPE = {
    .name="app_tcp_echo_msu",
    .id = PICO_TCP_APP_TCP_ECHO_ID,
    .init = msu_app_tcp_echo_init,
    .receive = msu_app_tcp_echo_receive,
};
