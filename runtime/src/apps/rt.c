/**
 * @file rt.c
 * The main executable file to start the runtime.
 */
#include <stdlib.h>
#include <stdio.h>

#include "communication.h"
#include "global_controller/dfg.h"
#include "dfg_interpreter.h"
#include "runtime.h"
#include "global.h"
#include "modules/ssl_read_msu.h"

#define USE_OPENSSL

#define USAGE_ARGS "<dfg.json> <runtime_id> <webserver_port> "\
                   "[db_ip db_port max_db_load]"

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

    if (argc < 4){
        printf("%s %s\n", argv[0], USAGE_ARGS);
        exit(0);
    }

    int runtime_id = atoi(argv[2]);
    int rtn = do_dfg_config(argv[1]);
    if ( rtn < 0 ) {
        printf("%s is not a valid DFG json file. Exiting.\n", argv[1]);
        exit(-1);
    }

    struct dfg_config *dfg = get_dfg();
    struct dfg_runtime_endpoint *rt = get_local_runtime(dfg, runtime_id);
    if ( rt == NULL ){
        printf("Runtime %d not present in provided DFG. Exiting.\n", runtime_id);
        exit(-1);
    }

    char *master_ip = dfg->global_ctl_ip;
    uint32_t master_ip_int;
    string_to_ipv4(master_ip, &master_ip_int);

    int master_port = dfg->global_ctl_port;
    runtime_listener_port = rt->port;
    int control_listen_port = runtime_listener_port;
    uint32_t this_ip = rt->ip;

    int same_physical_machine = ( master_ip_int == this_ip );
    int webserver_port = atoi(argv[3]);

    if (argc > 4) {
        // Declared in communication.h, used in webserver_msu
        db_ip = (char*)malloc(16);
        strncpy(db_ip, argv[4], strlen(argv[4]));
        db_port = atoi(argv[5]);
        db_max_load = atoi(argv[6]);

        // Initialize random number generator
        time_t t;
        srand((unsigned) time(&t));
    }

    log_info("Starting runtime...");

    // Control socket init for listening to connections
    // from other runtimes
    if (same_physical_machine == 1)
        control_listen_port++;

#ifdef USE_OPENSSL
    init_locks();
    InitServerSSLCtx(&ssl_ctx_global);
    if (LoadCertificates(ssl_ctx_global, "mycert.pem", "mycert.pem") < 0){
        log_error("Error loading SSL certificates");
    }
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

    dedos_main_thread_loop(dfg, runtime_id);

#ifdef USE_OPENSSL
    freeSSLRelatedStuff();
    kill_locks();
#endif
    free(db_ip);
    log_info("Runtime exit...");
    return 0;
}

