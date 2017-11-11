#include "webserver/sslops.h"
#include "webserver/webserver.h"
#include "logging.h"
#include <pthread.h>
#include <openssl/err.h>

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else 
#define UNUSED
#endif

//#define PRINT_REQUEST_ERRORS

#ifdef PRINT_REQUEST_ERRORS
#define print_request_error(fmt, ...)\
    log_error(fmt, ##__VA_ARGS__);
#else
#define print_request_error(...)
#endif

/**
 * Explains an SSL error. Written as a macro so we have traceability in the log_error statements
 */
//FIXME: calling ERR_get_error in the case SSL_ERROR_SSL breaks consistency of this function
// (which otherwise reads the stack and doesn't pop it)
#ifdef PRINT_REQUEST_ERRORS
#define print_ssl_error(ssl, rtn) { \
    int _err = SSL_get_error(ssl, rtn); \
    const char* UNUSED error_str = ERR_error_string(_err, NULL); \
    switch (_err) { \
        case SSL_ERROR_ZERO_RETURN: { \
            print_request_error("SSL_ERROR_ZERO_RETURN (%d): %s\n", _err, error_str); \
            break; \
        } \
        case SSL_ERROR_WANT_READ: { \
            print_request_error("SSL_ERROR_WANT_READ (%d): %s\n", _err, error_str);\
            break;\
        }\
        case SSL_ERROR_WANT_WRITE: {\
            print_request_error("SSL_ERROR_WANT_WRITE (%d): %s\n", _err, error_str);\
            break;\
        }\
        case SSL_ERROR_WANT_CONNECT: {\
            print_request_error("SSL_ERROR_WANT_CONNECT (%d): %s\n", _err, error_str);\
            break;\
        }\
        case SSL_ERROR_WANT_ACCEPT: {\
            print_request_error("SSL_ERROR_WANT_ACCEPT (%d): %s\n", _err, error_str);\
            break;\
        }\
        case SSL_ERROR_WANT_X509_LOOKUP: {\
            print_request_error("SSL_ERROR_WANT_X509_LOOKUP (%d): %s\n", _err, error_str);\
            break;\
        }\
        case SSL_ERROR_SYSCALL: {\
            print_request_error("SSL_ERROR_SYSCALL (%d): %s\n", _err, error_str);\
            break;\
        }\
        case SSL_ERROR_SSL: {\
            unsigned long UNUSED err2 = ERR_get_error(); \
            print_request_error("SSL_ERROR_SSL (%d, %lx): %s\n", _err, err2, error_str);\
            break;\
        }\
        default:\
            break;\
    }\
}
#else
#define print_ssl_error(ssl, rtn)
#endif

static pthread_mutex_t *lockarray;

static void crypto_locking_callback(int mode, int n,
        const char UNUSED *file, int UNUSED line) {
    if (mode & CRYPTO_LOCK) {
        pthread_mutex_lock(&(lockarray[n]));
    } else {
        pthread_mutex_unlock(&(lockarray[n]));
    }
}

static unsigned long crypto_id_function(void) {
    return (unsigned long)pthread_self();
}

void init_ssl_locks(void) {
#if defined(OPENSSL_THREADS)
#else
    log_error("no OPENSSL thread support!");
#endif
    lockarray = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(*lockarray));
    for (int i=0; i < CRYPTO_num_locks(); i++) {
        pthread_mutex_init(&(lockarray[i]), NULL);
    }
    CRYPTO_set_id_callback(crypto_id_function);
    CRYPTO_set_locking_callback(crypto_locking_callback);
}

SSL_CTX *ssl_ctx = NULL;

void kill_ssl_locks(void) {
    CRYPTO_set_locking_callback(NULL);
    for (int i=0; i<CRYPTO_num_locks(); i++) {
        pthread_mutex_destroy(&(lockarray[i]));
    }
    if (ssl_ctx != NULL) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }
}


int init_ssl_context(void) {
    if (ssl_ctx != NULL) {
        log_warn("SSL context already initialized. Not initializing again.");
        return 1;
    }

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    /* Create new SSL context */
    const SSL_METHOD *method = TLSv1_2_server_method();
    ssl_ctx = SSL_CTX_new(method);
    SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_OFF);
    SSL_CTX_set_options(ssl_ctx, 
            SSL_OP_NO_SSLv3 | SSL_OP_NO_TICKET | SSL_OP_SINGLE_DH_USE);

    int rtn = SSL_CTX_set_cipher_list(ssl_ctx, "HIGH:!aNULL:!MD5:!RC4");
    if ( rtn <= 0 ) {
        log_error("Could not set cipger list for SSL context");
        return -1;
    }

    return 0;
}

int load_ssl_certificate(char *cert_file, char *key_file) {
    /* set the local certificate from CertFile */
    if ( SSL_CTX_use_certificate_chain_file(ssl_ctx, cert_file) <= 0 ) {
        return -1;
    }
    /* set the private key from KeyFile (may be the same as CertFile) */
    if ( SSL_CTX_use_PrivateKey_file(ssl_ctx, cert_file, SSL_FILETYPE_PEM) <= 0 ) {
        return -1;
    }
    /* verify private key */
    if ( !SSL_CTX_check_private_key(ssl_ctx) ) {
        return -1;
    }
    return 0;
}
 
SSL *init_ssl_connection(int fd) {
    SSL *ssl = SSL_new(ssl_ctx);
    SSL_set_fd(ssl, fd);
    return ssl;
}

#define CHECK_SSL_WANTS(ssl, rtn) \
    do { \
        int err = SSL_get_error(ssl, rtn); \
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) { \
            print_ssl_error(ssl, rtn); \
            return WS_ERROR; \
        } else if ( err == SSL_ERROR_WANT_READ) { \
            return WS_INCOMPLETE_READ; \
        } else if ( err == SSL_ERROR_WANT_WRITE) { \
            return WS_INCOMPLETE_WRITE; \
        } \
    } while (0);

int accept_ssl(SSL *ssl) {
    ERR_clear_error();
    int rtn = SSL_accept(ssl);
    if (rtn == 0) {
        log_warn("The TLS/SSL handshake was not successful "
                 "but was shut down controlled and by the "
                 "specifications of the TLS/SSL protocol");
        print_ssl_error(ssl, rtn);
        return -1;
    }
    if (rtn < 0) {
        CHECK_SSL_WANTS(ssl, rtn);
    }
    return WS_COMPLETE;
}

int read_ssl(SSL *ssl, char *buf, int *buf_size) {
    ERR_clear_error();
    int num_bytes = SSL_read(ssl, buf, *buf_size);
    if (num_bytes > 0) {
        *buf_size = num_bytes;
        return WS_COMPLETE;
    } else {
        CHECK_SSL_WANTS(ssl, num_bytes);
    }
    log_warn("Shouldn't ever get here...");
    return WS_ERROR;
}

int write_ssl(SSL *ssl, char *buf, int *buf_size) {
    ERR_clear_error();
    int num_bytes = SSL_write(ssl, buf, *buf_size);
    if (num_bytes > 0) {
        if (num_bytes != *buf_size) {
            log_warn("Didn't write the requested amount (written: %d, requested: %d)...",
                     num_bytes, *buf_size);
        }
        *buf_size = num_bytes;
        return WS_COMPLETE;
    } else {
        CHECK_SSL_WANTS(ssl, num_bytes);
    }
    log_warn("Shouldn't ever get here...");
    return WS_ERROR;
}

int close_ssl(SSL *ssl) {
    ERR_clear_error();
    int rtn = SSL_get_shutdown(ssl);
    if (rtn >= 0) {
        rtn = SSL_shutdown(ssl);
        if (rtn == -1) {
            log(WS_ERROR, "Error shutting down SSL connection");
            SSL_free(ssl);
            return WS_ERROR;
        }
        if (rtn == 0) {
            //See the man page to understand this way of handling error on SSL_shutdown()
            //TODO: need too handle WANT_READ and WANT_WRITE on BOTH calls
            //see https://stackoverflow.com/questions/28056056/handling-ssl-shutdown-correctly
            rtn = SSL_shutdown(ssl);
            if (rtn == -1) {
        //        log_error("Error shutting down SSL connection");
                SSL_free(ssl);
                return WS_ERROR;
            }
        }
    }

    SSL_free(ssl);
    return WS_COMPLETE;
}
