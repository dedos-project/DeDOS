#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "dedos_msu_list.h"
#include "ssl_read_msu.h"
#include "runtime.h"
#include "webserver_msu.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"
#include "global.h"

int ReadSSL(SSL *State, char *Buffer, int BufferSize)
{
    int NumBytes;
    int ret, err;

    if ( (ret = SSL_accept(State)) < 0 ) {
        log_error("SSL_accept failed with ret = %d", ret);
        err = SSL_get_error(State, ret);
        SSLErrorCheck(err);

        return -1;
    }

    if ( (NumBytes = SSL_read(State, Buffer, BufferSize)) <= 0 ) {
        err = SSL_get_error(State, NumBytes);
        char *error;

        while (err == SSL_ERROR_WANT_READ) {
            //debug("SSL_read returned ret: %d. Errno: %s",
            //       NumBytes, error);
            NumBytes = SSL_read(State, Buffer, BufferSize);
            //strerror is not thread safe
            error = strerror(errno);
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
    Method = SSLv23_server_method();
    *Ctx = SSL_CTX_new(Method);
    SSL_CTX_set_options(*Ctx, SSL_OP_NO_SSLv3);

    //SSL_CTX_set_session_cache_mode(*Ctx, SSL_SESS_CACHE_OFF);
    //SSL_CTX_set_options(*Ctx, SSL_OP_NO_TICKET);
    //SSL_CTX_set_options(*Ctx, SSL_OP_SINGLE_DH_USE);
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

    SSL_CTX* ssl_read_msu_ctx = ssl_ctx_global;

    SSL *State = SSL_new(ssl_read_msu_ctx);
    if (State == NULL) {
        close(SocketFD);
        debug("ERROR: ssl msu id %d: SSL State creation failed", self->id);
        return NULL;
    }

    *SSL_State = State;
    if (SSL_set_fd(State, SocketFD) == 0) {
        close(SocketFD);
        debug("ERROR: ssl msu id %d: SSL connect to socket %d failed",
              self->id, SocketFD);
        return NULL;
    }

    debug("MSU %d calling SSL_read", self->id);
    int ReadBytes;
    if ( ( ReadBytes = ReadSSL(State, Request, MAX_REQUEST_LEN) ) < 0 ) {
        log_error("SSL_read on socket %d failed. Data read: %s", SocketFD, Request);
        SSL_free(State);
        close(SocketFD);
        return NULL;
    }

    return Request;
}

int ssl_read_receive(msu_t *self, msu_queue_item_t *input_data) {
    int ret = 0;
    struct ssl_data_payload *data = input_data->buffer;
    if (data->type == READ) {
        char *retState = GetSSLStateAndRequest(data->socketfd, &(data->state), self, data->msg);
        if (retState == NULL) {
            debug("DEBUG: ssl msu %d couldn't get data from socket %d", self->id, data->socketfd);
            return -1;
        }
        data->ipAddress = runtimeIpAddress;
        data->port = runtime_listener_port;
        data->sslMsuId = self->id;

        return DEDOS_WEBSERVER_MSU_ID;
    }
    log_debug("Freeing state in SSL MSU %s","");
    SSL_free(data->state);
    return -1;
}

msu_type_t SSL_READ_MSU_TYPE = {
    .name="SSL_Read_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_SSL_READ_MSU_ID,
    .proto_number=0,
    .init=NULL,
    .destroy=NULL,
    .receive=ssl_read_receive,
    .receive_ctrl=NULL,
    .route=round_robin,
    .deserialize=default_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote
};
