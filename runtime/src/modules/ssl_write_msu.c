#include <stdio.h>
#include <unistd.h>

#include "communication.h"
#include "modules/ssl_write_msu.h"
#include "runtime.h"
#include "modules/webserver_msu.h"
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
int ssl_write_receive(struct generic_msu *self, struct generic_msu_queue_item *input_data) {
    if (input_data != NULL) {
        struct ssl_data_payload *data = (struct ssl_data_payload*) input_data->buffer;
        WriteSSL(data->state, data->msg, strlen(data->msg));
        return 0;
    }
    return -1;
}

int ssl_write_route(struct msu_type *type, struct generic_msu *sender, struct generic_msu_queue_item *data){
    struct ssl_data_payload *ssl_data = (struct ssl_data_payload*)data->buffer;
    log_debug("Attempting to route to %d", ssl_data->ipAddress);
    uint32_t ipAddress = ssl_data->ipAddress == runtimeIpAddress ? 0 : ssl_data->ipAddress;
    return round_robin_within_ip(type, sender, ipAddress);
}

struct msu_type SSL_WRITE_MSU_TYPE = {
    .name="SSLWriteMsu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_SSL_WRITE_MSU_ID,
    .proto_number=0,
    .init=NULL,
    .destroy=NULL,
    .receive=ssl_write_receive,
    .receive_ctrl=NULL,
    .route=ssl_write_route,
    .deserialize=default_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote,
};


