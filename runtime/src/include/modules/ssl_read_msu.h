#ifndef __SSL_READ_MSU_H__
#define __SSL_READ_MSU_H__

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include "generic_msu.h"
#include "generic_msu_queue.h"
#include "generic_msu_queue_item.h"
#include "ssl_msu.h"

#define MAX_REQUEST_LEN 256
#define READ 0
#define WRITE 1

struct msu_type SSL_READ_MSU_TYPE;

void InitServerSSLCtx(SSL_CTX **ctx);
int LoadCertificates(SSL_CTX *Ctx, char *CertFile, char *KeyFile);
char* GetSSLStateAndRequest(int SocketFD, SSL** State, struct generic_msu *self, char *);
int ReadSSL(SSL *State, char *Buffer);
int AcceptSSL(SSL *State);
#endif /* __SSL_READ_MSU_H__ */
