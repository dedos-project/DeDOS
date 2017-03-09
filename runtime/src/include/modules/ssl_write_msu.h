#ifndef __ssl_write_msu_H__
#define __ssl_write_msu_H__

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include "ssl_msu.h"

#define MAX_REQUEST_LEN 1024
#define READ 0
#define WRITE 1

int WriteSSL(SSL *State, char *Buffer, int BufferSize);
msu_type_t SSL_WRITE_MSU_TYPE;

#endif /* __ssl_write_msu_H__ */
