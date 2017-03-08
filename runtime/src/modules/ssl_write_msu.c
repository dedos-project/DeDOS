#include <stdio.h>
#include <unistd.h>

#include "ssl_write_msu.h"
#include "runtime.h"
#include "webserver_msu.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_list.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"
#include "global.h"

int WriteSSL(SSL *State, char *Buffer, int BufferSize) {
    int NumBytes, SockFd, err;

    if ( ( NumBytes = SSL_write(State, Buffer, BufferSize) ) <= 0 ) {
        log_error("SSL_write failed with ret = %d", NumBytes);
        err = SSL_get_error(State, NumBytes);
        SSLErrorCheck(err);

        SockFd = SSL_get_fd(State);
        SSL_free(State);
        close(SockFd);
        return -1;
    }
    
    SockFd = SSL_get_fd(State);
    SSL_free(State);
    close(SockFd);

    return NumBytes;
}
int ssl_write_receive(msu_t *self, msu_queue_item_t *input_data) {
    if (input_data != NULL) {
        struct ssl_data_payload *data = (struct ssl_data_payload*) input_data->buffer;
        WriteSSL(data->state, data->msg, strlen(data->msg));
        return 0;
    }
    return -1;
}
msu_type_t SSL_WRITE_MSU_TYPE = {
    .name="SSLWriteMsu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_SSL_WRITE_MSU_ID,
    .proto_number=0,
    .init=NULL,
    .destroy=NULL,
    .receive=ssl_write_receive,
    .receive_ctrl=NULL,
    .route=round_robin,
    .deserialize=default_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote,
};


