#ifndef SSL_OPS_H_
#define SSL_OPS_H_
#include <openssl/ssl.h>

void init_ssl_locks(void);
void kill_ssl_locks(void);

int init_ssl_context(void);
int load_ssl_certificate(char *cert_file, char *key_file);

SSL *init_ssl_connection(int fd);
int accept_ssl(SSL *ssl);
int read_ssl(SSL *ssl, char *buf, int *buf_size);
int write_ssl(SSL *ssl, char *buf, int *buf_size);
int close_ssl(SSL *ssl);

#endif
