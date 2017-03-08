/* PicoTCP Test application */
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

#include "generic_msu.h"
#include "communication.h"
#include "dedos_msu_list.h"

#include "runtime.h"
#include "logging.h"
#include "global.h"
#include "msu_tcp_handshake.h"
#include "ssl_msu.h"

#define USE_OPENSSL
static pthread_mutex_t *lockarray;

void app_udpecho(char *args);
void app_tcpecho(char *args);
void app_udpclient(char *args);
void app_tcpclient(char *args);
void app_tcpbench(char *args);
void app_natbox(char *args);
void app_udpdnsclient(char *args);
void app_udpnatclient(char *args);
void app_mcastsend(char *args);
void app_mcastreceive_ipv6(char *args);
void app_mcastsend_ipv6(char *args);
void app_mcastreceive(char *args);
void app_ping(char *args);
void app_dhcp_server(char *args);
void app_dhcp_client(char *args);
void app_dns_sd(char *arg, struct pico_ip4 addr);
void app_mdns(char *arg, struct pico_ip4 addr);
void app_sntp(char *args);
void app_tftp(char *args);
void app_slaacv4(char *args);
void app_udpecho(char *args);
void app_sendto_test(char *args);
void app_noop(void);

#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>

SSL_CTX* ssl_ctx_global;

struct pico_ip4 ZERO_IP4 = {
    0
};
struct pico_ip_mreq ZERO_MREQ = {
    .mcast_group_addr = {0}, .mcast_link_addr = {0}
};
struct pico_ip_mreq_source ZERO_MREQ_SRC = { {0}, {0}, {0} };
struct pico_ip6 ZERO_IP6 = {
 { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
struct pico_ip_mreq ZERO_MREQ_IP6 = {
    .mcast_group_addr.ip6 =  {{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }} ,
    .mcast_link_addr.ip6 = {{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }}
};
struct pico_ip_mreq_source ZERO_MREQ_SRC_IP6 = {
    .mcast_group_addr.ip6 = {{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }} ,
    .mcast_link_addr.ip6 =  {{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }} ,
    .mcast_source_addr.ip6 ={{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }} };

/* #define INFINITE_TCPTEST */
#define picoapp_dbg(...) do {} while(0)
/* #define picoapp_dbg printf */

/* #define PICOAPP_IPFILTER 1 */

int IPV6_MODE;


struct pico_ip4 inaddr_any = {
    0
};
struct pico_ip6 inaddr6_any = {{0}};

char *cpy_arg(char **dst, char *str);

char *cpy_arg(char **dst, char *str){
    ;
}

void deferred_exit(pico_time __attribute__((unused)) now, void *arg)
{
    if (arg) {
        free(arg);
        arg = NULL;
    }

    printf("%s: quitting\n", __FUNCTION__);
    exit(0);
}

#ifdef USE_OPENSSL
#include <openssl/crypto.h>
static void lock_callback(int mode, int type, char *file, int line) {
  (void) file;
  (void) line;
  if (mode & CRYPTO_LOCK) {
    pthread_mutex_lock(&(lockarray[type]));
  }
  else {
    pthread_mutex_unlock(&(lockarray[type]));
  }
}

static unsigned long thread_id(void) {
  unsigned long ret;

  ret=(unsigned long)pthread_self();
  return ret;
}

static void init_locks(void) {
  int i;

  lockarray = (pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() *
                                            sizeof(pthread_mutex_t));
  for (i = 0; i < CRYPTO_num_locks(); i++) {
    pthread_mutex_init(&(lockarray[i]), NULL);
  }

  CRYPTO_set_id_callback((unsigned long (*)())thread_id);
  CRYPTO_set_locking_callback((void (*)())lock_callback);
}

static void kill_locks(void) {
  int i;

  CRYPTO_set_locking_callback(NULL);
  for(i=0; i<CRYPTO_num_locks(); i++)
    pthread_mutex_destroy(&(lockarray[i]));

  OPENSSL_free(lockarray);
}
#endif


void freeSSLRelatedStuff(void) {
    log_debug("Freeing SSL related stuff%s","");
    CONF_modules_free();
    ERR_remove_state(0);
    ENGINE_cleanup();
    CONF_modules_unload(1);
    ERR_free_strings();
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
}

int main(int argc, char **argv){
    // initialize the context and read the certificates

    int master_port;
    if(argc < 5){
        printf("Usage: picoapp.elf master_ip master_port local_listen_port same_physical_machine webserver_port db_ip db_port max_db_load\n");
        exit(0);
    }

#ifdef PICO_SUPPORT_MUTEX
    log_debug("%s","PICO_SUPPORT_MUTEX");
#endif

    char *master_ip = argv[1];
    master_port = atoi(argv[2]);
    runtime_listener_port = atoi(argv[3]);
    int same_physical_machine = atoi(argv[4]);
    int webserver_port = atoi(argv[5]);
    int control_listen_port = 0;
    if (argc == 9) {
        db_ip = (char *) malloc(16);
        strncpy(db_ip, argv[6], strlen(argv[6]));
        db_port = atoi(argv[7]);
        db_max_load = atoi(argv[8]);
        // initialize random number generator
        time_t t;
        srand((unsigned) time(&t));
    }

    control_listen_port = runtime_listener_port;

    log_debug("%s","Starting runtime...");

     // Control socket init for listening to connections from other runtimes
    if(same_physical_machine == 1){
        control_listen_port++;
    }

    // initialize the certificates and stuff
#ifdef USE_OPENSSL
    init_locks();

    SSL_CTX *ctx;
    InitServerSSLCtx(&ctx);
    LoadCertificates(ctx, "mycert.pem", "mycert.pem");
    ssl_ctx_global = ctx;
    init_peer_socks();
#endif

    dedos_control_socket_init(control_listen_port);

    // init the webserver socket
    dedos_webserver_socket_init(webserver_port);

    /* connect to global master over TCP */
    int ret = 0;
    do{
    	ret = connect_to_master(master_ip, master_port);
    	sleep(2);
    }while(ret);

    log_info("%s","Connected to global master");

    /* runs networking I/O via select calls over sockets */
    dedos_main_thread_loop();


#ifdef USE_OPENSSL
    freeSSLRelatedStuff();
    kill_locks();
#endif
    free(db_ip);
    log_critical("Runtime exit...%s","");

    return 0;

}
