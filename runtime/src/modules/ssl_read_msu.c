#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "communication.h"
#include "dedos_msu_list.h"
#include "modules/ssl_read_msu.h"
#include "runtime.h"
#include "modules/webserver_msu.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"
#include "global.h"

int AcceptSSL(SSL *State) {
    int NumBytes;
    int ret, err;

    do {
        ERR_clear_error();
        ret = SSL_accept(State);
        err = 0;

        if (ret == 0) {
            log_warn("TLS/SSL handshake was not successful but was shut down controlled and by the \
                      specifications of the TLS/SSL protocol");
            SSLErrorCheck(err);
            return -1;
        }

        if (ret < 0) {
            //log_warn("SSL_accept failed with ret = %d", ret);
            err = SSL_get_error(State, ret);

            if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                SSLErrorCheck(err);
                return -1;
            } else {
                if (err == SSL_ERROR_WANT_READ) {
                    //log_warn("SSL_accept got SSL_ERROR_WANT_READ");
                } else {
                    log_warn("SSL_accept got SSL_ERROR_WANT_WRITE");
                }
            }
        }
    } while (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE);

    return 0;
}

int ReadSSL(SSL *State, char *Buffer) {
    int NumBytes;
    int ret, err;

    ERR_clear_error();
    if ( (NumBytes = SSL_read(State, Buffer, MAX_REQUEST_LEN)) <= 0 ) {
        err = SSL_get_error(State, NumBytes);

        char error[256];

        if (err != SSL_ERROR_WANT_READ) {
            SSLErrorCheck(err);
            return -1;
        }

        while (err == SSL_ERROR_WANT_READ) {
            ERR_error_string(err, error);
            //log_warn("SSL_read returned ret: %d. %s", NumBytes, error);
            ERR_clear_error();
            NumBytes = SSL_read(State, Buffer, MAX_REQUEST_LEN);

            err = SSL_get_error(State, NumBytes);
        }

        SSLErrorCheck(err);
    }

    return NumBytes;
}

void InitServerSSLCtx(SSL_CTX **Ctx) {
    const SSL_METHOD *Method;

    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();

    /* create new SSL context */
    //Method = SSLv23_server_method();
    Method = TLSv1_2_server_method();
    *Ctx = SSL_CTX_new(Method);

    SSL_CTX_set_session_cache_mode(*Ctx, SSL_SESS_CACHE_OFF);
    SSL_CTX_set_options(*Ctx, SSL_OP_NO_SSLv3 | SSL_OP_NO_TICKET | SSL_OP_SINGLE_DH_USE);

    int rc;
    rc = SSL_CTX_set_cipher_list(*Ctx, "HIGH:!aNULL:!MD5:!RC4");
    assert(rc >= 1);

    //SSL_CTX_set_options(*Ctx, SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION);
    //SSL_CTX_set_options(*Ctx, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
    //SSL_CTX_set_mode(*Ctx, SSL_MODE_AUTO_RETRY);

    if (*Ctx == NULL) {
        ERR_print_errors_fp(stderr);
    }
}


int LoadCertificates(SSL_CTX *Ctx, char *CertFile, char *KeyFile)
{
    // /* set the local certificate from CertFile */
    int ret;
    if ( (ret = SSL_CTX_use_certificate_file(Ctx, CertFile, SSL_FILETYPE_PEM)) <= 0 ) {
        return -1;
    }
    // printf("Finished 1st step LoadCertificates\n");
    //  set the private key from KeyFile (may be the same as CertFile)
    if ( SSL_CTX_use_PrivateKey_file(Ctx, KeyFile, SSL_FILETYPE_PEM) <= 0 ) {
        return -1;
    }
    // /* verify private key */
    if ( !SSL_CTX_check_private_key(Ctx) ) {
        return -1;
    }
    return 0;
}

char* GetSSLStateAndRequest(int SocketFD, SSL **SSL_State, struct generic_msu *self, char *Request)
{
    // while incoming queue is not empty, process and enqueue the state, fd to afterSSlqueue

    SSL *State = init_ssl_connection(SocketFD);
    *SSL_State = State;

    int rtn;

    rtn = AcceptSSL(State);
    if (rtn < 0) {
        SSL_shutdown(State);
        SSL_free(State);
        close(SocketFD);
        log_error("Error accepting SSL");

        return NULL;
    }

    rtn = ReadSSL(State, Request);
    if (rtn < 0) {
        SSL_shutdown(State);
        SSL_free(State);
        close(SocketFD);
        log_error("Error reading SSL");

        return NULL;
    }


    return Request;
}

static int ssl_read_receive(struct generic_msu *self, struct generic_msu_queue_item *input_data) {
    int ret = 0;
    struct ssl_data_payload *data = input_data->buffer;
    if (data->type == READ) {
        char *retState = GetSSLStateAndRequest(data->socketfd, &(data->state), self, data->msg);
        if (retState == NULL) {
            log_warn("DEBUG: ssl msu %d couldn't get data from socket %d", self->id, data->socketfd);
            return -1;
        }
        data->ipAddress = runtimeIpAddress;
        data->port = runtime_listener_port;
        data->sslMsuId = self->id;

        log_debug("IP address %d", data->ipAddress);
        log_debug("SSL state location: %p", data->state);
        log_debug("SSL state version: %d", data->state->version);
        return DEDOS_WEBSERVER_MSU_ID;
    }
    log_debug("Freeing state in SSL MSU %s","");
    SSL_free(data->state);
    return -1;
}

struct msu_type SSL_READ_MSU_TYPE = {
    .name="SSL_Read_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_SSL_READ_MSU_ID,
    .proto_number=0,
    .init=NULL,
    .destroy=NULL,
    .receive=ssl_read_receive,
    .receive_ctrl=NULL,
    .route=shortest_queue_route,
    .deserialize=default_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote
};
