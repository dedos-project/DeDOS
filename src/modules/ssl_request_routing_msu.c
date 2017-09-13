#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "runtime.h"
#include "modules/ssl_request_routing_msu.h"
#include "modules/ssl_read_msu.h"
#include "modules/ssl_msu.h"
#include "modules/ssl_write_msu.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_list.h"
#include "dedos_msu_msg_type.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"
#include "global.h"
#include "hash_function.h"
#include "data_plane_profiling.h"

static unsigned concatenate(unsigned x, unsigned y) {
    unsigned pow = 10;
    while(y >= pow) {
        pow *= 10;
    }
    return x * pow + y;
}

int ssl_request_routing_msu_receive(struct generic_msu *self,
                                    struct generic_msu_queue_item *queue_item) {

    /* function called when an item is dequeued */
    /* here we will make a decision as to which next SSL msu the request be routed to */
    struct ssl_data_payload *data;
    int ret = 0;
    int next_msu_id;
    struct chord_node* dst_chord_node;
    unsigned int derived_key;

    data = (struct ssl_data_payload*) queue_item->buffer;

    struct msu_endpoint *tmp_dst_endpoint;
    struct msu_endpoint *all_msu_enpoints = NULL;
    int next_msu_type = DEDOS_SSL_READ_MSU_ID;

    /**
     * Compute a 32bit int key based on client ip and port
     */

    /*
    struct sockaddr_in client;
    socklen_t len;

    if (getpeername(data->socketfd, (struct sockaddr *) &client, &len) < 0) {
        log_error("ERROR: %s", "Unable to retrieve client's information from client socket");
        return -1;
    }

    derived_key =
        compute_hash(concatenate(ntohs(client.sin_addr.s_addr), ntohs(client.sin_port)));
    log_debug("Derived key for SSL request: %u", derived_key);

    // do a lookup to get next_msu_id
    dst_chord_node = lookup_key(self->internal_state, derived_key);
    next_msu_id = dst_chord_node->next_msu_id;
    log_debug("Picked next SSL MSU id: %d", next_msu_id);

    tmp_dst_endpoint = dst_chord_node->msu_endpoint;

    if(tmp_dst_endpoint->id != next_msu_id){
        log_error("Mismatch in id's given by chord (%d) and routing table entry (%d)",next_msu_id, tmp_dst_endpoint->id);
        return -1;
    }
    */
    return next_msu_type;
}

int ssl_init(struct generic_msu *self, void *init_data, int init_data_len){
    ssl_request_routing_msu = self;
    return 0;
}

/**
 * Sets the ID of the incoming queue item to be a hash of the client's IP and port
 * @param self The ssl-request-routing MSU
 * @param queue_item the queue item on which the ID is to be set
 * @return queue item's ID
 */
int ssl_generate_id(struct generic_msu *self, struct generic_msu_queue_item *queue_item){
    struct ssl_data_payload *data = queue_item->buffer;
    struct sockaddr_in sockaddr;
    socklen_t addrlen = sizeof(sockaddr);
    // Get the client's IP and port
    int rtn = getpeername(data->socketfd, (struct sockaddr*) &sockaddr, &addrlen);
    if (rtn < 0){
        log_error("Could not getpeername for determining packet ID: %s", strerror(errno));
    }
    uint32_t id;
    log_debug("Sockaddr port: %d, addr %d", sockaddr.sin_port, sockaddr.sin_addr.s_addr);
    // Hash the client's address into the id
    HASH_VALUE(&sockaddr, addrlen, id);
    return id;
}

const struct msu_type SSL_REQUEST_ROUTING_MSU_TYPE = {
    .name="ssl_request_routing_msu",
    .layer=DEDOS_LAYER_TRANSPORT,
    .type_id=DEDOS_SSL_REQUEST_ROUTING_MSU_ID,
    .proto_number=MSU_PROTO_SSL_REQUEST,
    .init=ssl_init,
    .destroy=NULL,
    .receive=ssl_request_routing_msu_receive,
    .receive_ctrl=NULL,
    .route=default_routing,
    .deserialize=default_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote,
    .generate_id=ssl_generate_id
};
