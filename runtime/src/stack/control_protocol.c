#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "control_protocol.h"
#include "logging.h"
#include "dedos_thread_queue.h"

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
