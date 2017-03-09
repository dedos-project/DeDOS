#ifndef __SSL_MSU_H__
#define __SSL_MSU_H__

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include "generic_msu.h"
#include "generic_msu_queue.h"
#include "generic_msu_queue_item.h"

#define MAX_REQUEST_LEN 1024
#define READ 0
#define WRITE 1

static void SSLErrorCheck (int err) {
    const char* error_str = ERR_error_string(err, NULL);
    switch (err) {
        case SSL_ERROR_ZERO_RETURN: {
            log_error("SSL_ERROR_ZERO_RETURN (%d): %s", err, error_str);
            break;
        }
        case SSL_ERROR_WANT_READ: {
            log_error("SSL_ERROR_WANT_READ (%d): %s", err, error_str);
            break;
        }
        case SSL_ERROR_WANT_WRITE: {
            log_error("SSL_ERROR_WANT_WRITE (%d): %s", err, error_str);
            break;
        }
        case SSL_ERROR_WANT_CONNECT: {
            log_error("SSL_ERROR_WANT_CONNECT (%d): %s", err, error_str);
            break;
        }
        case SSL_ERROR_WANT_ACCEPT: {
            log_error("SSL_ERROR_WANT_ACCEPT (%d): %s", err, error_str);
            break;
        }
        case SSL_ERROR_WANT_X509_LOOKUP: {
            log_error("SSL_ERROR_WANT_X509_LOOKUP (%d): %s", err, error_str);
            break;
        }
        case SSL_ERROR_SYSCALL: {
            log_error("SSL_ERROR_SYSCALL (%d): %s", err, error_str);
            break;
        }
        case SSL_ERROR_SSL: {
            log_error("SSL_ERROR_SSL (%d): %s", err, error_str);
            break;
        }
        default:
            break;
    }
}

struct ssl_data_payload {
    int sslMsuId;
    int type;
    int socketfd;
    uint32_t ipAddress;
    int port;
    char msg[MAX_REQUEST_LEN];
    SSL *state;
};

#endif /* __SSL_MSU_H__ */
