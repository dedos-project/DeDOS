#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "control_protocol.h"
#include "logging.h"

struct dedos_thread_msg* dedos_thread_msg_alloc(){
    struct dedos_thread_msg* msg;
    msg = (struct dedos_thread_msg*)malloc(sizeof(struct dedos_thread_msg));
    if(!msg){
        log_error("Failed to allocate dedos_thread_msg %s",strerror(errno));
        return NULL;
    }
    msg->next = NULL;
    return msg;
}

void dedos_thread_msg_free(struct dedos_thread_msg* msg){

    if(msg->data){
        free(msg->data);
    }
    free(msg);

}

int send_to_endpoint(int endpoint_sock, char *buf, unsigned int bufsize){
    log_debug("Sending msg to peer runtime with len: %u", bufsize);
    int data_len;
    data_len = (int)send(endpoint_sock, buf, bufsize, 0);
    if (data_len == -1) {
        log_error("failed to send data on socket");
        return -1;
    }
    if (data_len != bufsize) {
        log_warn("Failed to send full message to socket %d", endpoint_sock);
        return -1;
    }
    log_info("Sent msg to peer runtime with len: %d", data_len);
    return 0;
}

/**
 * Serializes a control message (assuming payload is already serialized) and sends to the
 * global controller or runtime indicated by the given socket number
 * @param sock The socket to which to send the control message
 * @param control_msg The control message to send
 * @return 0 on success, -1 on error
 */
int send_control_msg(int sock, struct dedos_control_msg *control_msg) {
    int total_msg_size = control_msg->header_len + control_msg->payload_len;

    char buf[total_msg_size];

    memcpy(buf, control_msg, control_msg->header_len);

    if (control_msg->payload_len > 0) {
        memcpy(buf + control_msg->header_len, control_msg->payload, control_msg->payload_len);
    }

    return send_to_endpoint(sock, buf, (unsigned int)total_msg_size);
}

