/*********************************************************************
   PicoTCP. Copyright (c) 2012-2015 Altran Intelligent Systems. Some rights reserved.
   See LICENSE and COPYING for usage.

   Authors: Daniele Lacamera
 *********************************************************************/

#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <signal.h>
#include <pcap.h>
#include "pico_device.h"
#include "pico_dev_pcap.h"
#include "pico_stack.h"
#include "legacy_logging.h"

#ifndef __FreeBSD__
#include <linux/if_tun.h>
#endif

#include <sys/poll.h>

struct pico_device_pcap {
    struct pico_device dev;
    pcap_t *conn;
};

#define VDE_MTU 2048

static int pico_pcap_send(struct pico_device *dev, void *buf, int len)
{
    struct pico_device_pcap *pcap = (struct pico_device_pcap *) dev;
    /* dbg("[%s] send %d bytes.\n", dev->name, len); */
    return pcap_inject(pcap->conn, buf, (uint32_t)len);
}

static void pico_dev_pcap_cb(u_char *u, const struct pcap_pkthdr *h, const u_char *data)
{
    struct pico_device *dev = (struct pico_device *)u;
    uint8_t *buf = (uint8_t *)data;
    pico_stack_recv(dev, buf, (uint32_t)h->len);
}


static int pico_pcap_poll(struct pico_device *dev, int loop_score)
{
    struct pico_device_pcap *pcap = (struct pico_device_pcap *) dev;
    loop_score -= pcap_dispatch(pcap->conn, loop_score, pico_dev_pcap_cb, (u_char *) pcap);
    return loop_score;
}

static int pcap_get_mac(char *name, uint8_t *mac)
{
    int sck;
    struct ifreq eth;
    int retval = -1;




    sck = socket(AF_INET, SOCK_DGRAM, 0);
    if(sck < 0) {
        return retval;
    }

    memset(&eth, 0, sizeof(struct ifreq));
    strcpy(eth.ifr_name, name);
    /* call the IOCTL */
    if (ioctl(sck, SIOCGIFHWADDR, &eth) < 0) {
        perror("ioctl(SIOCGIFHWADDR)");
        return -1;
        ;
    }

    memcpy (mac, &eth.ifr_hwaddr.sa_data, 6);


    close(sck);
    return 0;

}

/* Public interface: create/destroy. */

void pico_pcap_destroy(struct pico_device *dev)
{
    struct pico_device_pcap *pcap = (struct pico_device_pcap *) dev;
    pcap_close(pcap->conn);
}

#define PICO_PCAP_MODE_LIVE 0
#define PICO_PCAP_MODE_STORED 1

static struct pico_device *pico_pcap_create(char *if_file_name, char *name, uint8_t *mac, int mode)
{
    struct pico_device_pcap *pcap = PICO_ZALLOC(sizeof(struct pico_device_pcap));
    char errbuf[2000];
    if (!pcap)
        return NULL;

    if( 0 != pico_device_init((struct pico_device *)pcap, name, mac)) {
        dbg ("Pcap init failed.\n");
        pico_pcap_destroy((struct pico_device *)pcap);
        return NULL;
    }

    pcap->dev.overhead = 0;

    if (mode == PICO_PCAP_MODE_LIVE){
        int status;
        pcap->conn = pcap_create(if_file_name, errbuf);

        if (pcap->conn == NULL){
            log_error("error creating pcap dev");
            goto end;
        }
        status = pcap_set_snaplen(pcap->conn, 2000);
        if (status < 0)
            log_error("error setting snaplen in pcap");

        status = pcap_set_promisc(pcap->conn, 1);
        if (status < 0)
            log_error("error setting promiscuous mode");

        status = pcap_set_timeout(pcap->conn, 1);
        if (status < 0)
            log_error("error setting pcap timeout");

        status = pcap_set_buffer_size(pcap->conn, 1000<<20);
        if(status < 0)
            log_error("Error setting pcap buffer size");

        status = pcap_activate(pcap->conn);
        if (status < 0){
            log_error("Failed to activate pcap dev");
            goto end;
        }
//        pcap->conn = pcap_open_live(if_file_name, 2000, 1, 1, errbuf);

    } else {
        pcap->conn = pcap_open_offline(if_file_name, errbuf);
    }
end:
    if (!pcap->conn) {
        pico_pcap_destroy((struct pico_device *)pcap);
        return NULL;
    }
    pcap->dev.send = pico_pcap_send;
    pcap->dev.poll = pico_pcap_poll;
    pcap->dev.destroy = pico_pcap_destroy;
    dbg("Device %s created.\n", pcap->dev.name);
    return (struct pico_device *)pcap;
}

struct pico_device *pico_pcap_create_fromfile(char *filename, char *name, uint8_t *mac)
{
    return pico_pcap_create(filename, name, mac, PICO_PCAP_MODE_STORED);
}

struct pico_device *pico_pcap_create_live(char *ifname, char *name, uint8_t *mac)
{
    uint8_t mac_new[6] = {};
    if(mac == NULL){
        if (pcap_get_mac(ifname, mac_new) < 0) {
            dbg("PCAP mac query failed.\n");
            return NULL;
        }
        return pico_pcap_create(ifname, name,  mac_new, PICO_PCAP_MODE_LIVE);
    }
    return pico_pcap_create(ifname, name, mac, PICO_PCAP_MODE_LIVE);
}
