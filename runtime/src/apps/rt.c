/**
 * @file rt.c
 * The main executable file to start the runtime.
 */
#include <stdlib.h>
#include <stdio.h>

#include "stats.h"
#include "communication.h"
#include "global_controller/dfg.h"
#include "dfg_interpreter.h"
#include "runtime.h"
#include "global.h"
#include "modules/ssl_read_msu.h"

#define USE_OPENSSL

#define USAGE_ARGS " [-j dfg.json -i runtime_id] | " \
                   "[-g global_ctl_ip -p global_ctl_port -P local_listen_port [--same-machine | -s]] "\
                   "-w webserver_port [--db-ip db_ip --db-port db_port --db-load db_max_load] "

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
#ifdef DATAPLANE_PROFILING
    log_warn("Data plane profiling enabled");
    if (pthread_mutex_init(&request_id_mutex, NULL) != 0)
    {
        log_error("request id mutex init failed");
        return 1;
    }
#endif
    char *dfg_json = NULL;
    int runtime_id = -1;
    char *global_ctl_ip = NULL;
    int global_ctl_port = -1;
    int local_listen_port = -1;
    int same_physical_machine = 0;
    int webserver_port = -1;
    // Declared in communication.h, used in webserver_msu
    db_ip = NULL;
    db_port = -1;
    db_max_load = -1;

    struct option long_options[] = {
        {"same-machine", no_argument, 0, 's'},
        {"db-ip", required_argument, 0, 'd'},
        {"db-port", required_argument, 0, 'b'},
        {"db-load", required_argument, 0, 'l'},
        {0, 0, 0, 0}
    };

    int arguments_provided = 0;
    while (1){
        int option_index = 0;
        int c = getopt_long(argc, argv, "j:i:g:p:P:s:w:d:b:l:",
                            long_options, &option_index);
        if (c == -1)
            break;
        arguments_provided = 1;

        switch (c){
            case 'j':
                dfg_json = optarg;
                break;
            case 'i':
                runtime_id = atoi(optarg);
                break;
            case 'g':
                global_ctl_ip = optarg;
                break;
            case 'p':
                global_ctl_port = atoi(optarg);
                break;
            case 'P':
                local_listen_port = atoi(optarg);
                break;
            case 's':
                same_physical_machine = 1;
                break;
            case 'w':
                webserver_port = atoi(optarg);
                break;
            case 'd':
                db_ip = optarg;
                break;
            case 'b':
                db_port = atoi(optarg);
                break;
            case 'l':
                db_max_load = atoi(optarg);
                break;
            case '?':
            default:
                printf("Unknown option provided 0%o. Exiting.\n", c);
                exit(-1);
        }
    }
    if (!arguments_provided){
        printf("%s %s\n", argv[0], USAGE_ARGS);
        exit(0);
    }
    char statlog_fname[48];
    sprintf(statlog_fname, "rt%d_stats.log", runtime_id);
    init_statlog(statlog_fname);
    int json_all = (dfg_json != NULL && runtime_id > 0);
    int json_any = (dfg_json != NULL || runtime_id > 0);

    int manual_all = (global_ctl_ip != NULL && global_ctl_port > 0 &&
                      local_listen_port > 0 && same_physical_machine > 0);
    int manual_any = (global_ctl_ip != NULL || global_ctl_port > 0 ||
                      local_listen_port > 0 || same_physical_machine > 0) ;

    int db_all = (db_ip != NULL && db_port > 0 && db_max_load > 0);

    if (!json_all && !manual_all){
        printf("One of JSON file and runtime ID or global control IP and port required. Exiting.\n");
        exit(-1);
    }
    if ((json_all && manual_any) || (json_any && manual_all)){
        printf("Both JSON and manual configuration present. Please provide only one.\n");
        exit(-1);
    }
    if (webserver_port == -1){
        printf("Webserver port not provided. Exiting\n");
        exit(-1);
    }

    struct dfg_config *dfg = NULL;
    if (json_all){
        int rtn = do_dfg_config(dfg_json);
        if (rtn < 0){
            printf("%s is not a valid json DFG. Exiting\n", dfg_json);
            exit(-1);
        }
        dfg = get_dfg();
        struct dfg_runtime_endpoint *rt = get_local_runtime(dfg, runtime_id);
        if (rt == NULL){
            printf("Runtime %d not present in provided DFG. Exiting.\n", runtime_id);
            exit(-1);
        }

        global_ctl_ip = dfg->global_ctl_ip;
        global_ctl_port = dfg->global_ctl_port;
        local_listen_port = rt->port;

        uint32_t global_ctl_ip_int;
        string_to_ipv4(global_ctl_ip, &global_ctl_ip_int);
        same_physical_machine = ( global_ctl_ip_int == rt->ip );

    }

    runtime_listener_port = local_listen_port;
    int control_listen_port = local_listen_port;

    // Control socket init for listening to connections from other runtimes
    if (same_physical_machine == 1){
        control_listen_port++;
    }

    if ( ! db_all){
        log_warn("Connection to mock database not fully instantiated");
    } else {
        // Initialize random number generator
        time_t t;
        srand((unsigned) time(&t));
    }
    log_info("Starting runtime...");


#ifdef USE_OPENSSL
    init_locks();
    InitServerSSLCtx(&ssl_ctx_global);
    if (LoadCertificates(ssl_ctx_global, "mycert.pem", "mycert.pem") < 0){
        log_error("Error loading SSL certificates");
    }
#endif
    init_peer_socks();

    if (dedos_control_socket_init(control_listen_port) < 0){
       log_error("Could not initialize control socket");
    }

    if (dedos_webserver_socket_init(webserver_port) < 0){
        log_error("Could nto initialize webserver socket");
    }

#ifdef DEDOS_SUPPORT_BAREMETAL_MSU
    if (dedos_baremetal_listen_socket_init(8989) < 0){
        log_error("Could not initialize baremetal listen socket");
    }
#endif
    int ret = 0;
    do {
        ret = connect_to_master(global_ctl_ip, global_ctl_port);
        sleep(2);
    } while (ret);

    log_info("Connected to global master");

    dedos_main_thread_loop(dfg, runtime_id);

#ifdef USE_OPENSSL
    freeSSLRelatedStuff();
    kill_locks();
#endif
    close_statlog();
    log_info("Runtime exit...");
    return 0;
}
