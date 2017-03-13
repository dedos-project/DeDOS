#include <stdlib.h>
#include <stdio.h>

#include "runtime.h"
#include "communication.h"
#include "global.h"
#include "modules/ssl_read_msu.h"

#define USE_OPENSSL

#define USAGE_ARGS "master_ip master_port local_listen_port " \
                   "same_physical_machine [webserver_port " \
                   "db_ip db_port max_db_load]"

SSL_CTX* ssl_ctx_global;
static pthread_mutex_t *lockarray;

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


#ifdef USE_OPENSSL
#include <openssl/crypto.h>
static void lock_callback(int mode, int type, const char *file, int line) {
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
  CRYPTO_set_locking_callback(lock_callback);
}

static void kill_locks(void) {
  int i;

  CRYPTO_set_locking_callback(NULL);
  for(i=0; i<CRYPTO_num_locks(); i++)
    pthread_mutex_destroy(&(lockarray[i]));

  OPENSSL_free(lockarray);
}
#endif

int main(int argc, char **argv){
    // initialize the context and read the certificates

    if (argc < 5){
        printf("%s %s\n", argv[0], USAGE_ARGS);
        exit(0);
    }

    char *master_ip = argv[1];
    int master_port = atoi(argv[2]);

    // Declared in communication.h, used in ssl_read_msu
    runtime_listener_port = atoi(argv[3]);
    int control_listen_port = runtime_listener_port;

    int same_physical_machine = atoi(argv[4]);
    int webserver_port = atoi(argv[5]);

    if (argc == 9) {
       // Declared in communication.h, used in webserver_msu
       db_ip = (char*)malloc(16);
       strncpy(db_ip, argv[6], strlen(argv[6]));
       db_port = atoi(argv[7]);
       db_max_load = atoi(argv[8]);

       // Initialize random number generator
       time_t t;
       srand((unsigned) time(&t));
    }

    log_info("Starting runtime...");

    // Control socket init for listening to connections
    // from other runtimes
    if (same_physical_machine == 1)
        control_listen_port++; // IMP: I do not understand

#ifdef USE_OPENSSL
    init_locks();
    InitServerSSLCtx(&ssl_ctx_global);
    LoadCertificates(ssl_ctx_global, "mycert.pem", "mycert.pem");
#endif
    init_peer_socks();

    dedos_control_socket_init(control_listen_port);

    dedos_webserver_socket_init(webserver_port);

    int ret = 0;
    do {
        ret = connect_to_master(master_ip, master_port);
        sleep(2);
    } while (ret);

    log_info("Connected to global master");

    dedos_main_thread_loop();

#ifdef USE_OPENSSL
    freeSSLRelatedStuff();
    kill_locks();
#endif
    free(db_ip);
    log_info("Runtime exit...");
    return 0;
}

