/**
 * ssl_msu.c
 *
 * Functions used across all ssl msus
 */

#include "modules/ssl_msu.h"

/**
 * Checks and logs an error number returned from SSL operations
 * @param err Error number
 */
void SSLErrorCheck(int err){
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
