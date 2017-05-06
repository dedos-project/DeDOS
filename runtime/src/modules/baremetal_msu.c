/**
 * baremetal_msu.c
 *
 * MSU for evaluating runtime performance and mock computational
 * delay within MSU
 */
#include "runtime.h"
#include "modules/baremetal_msu.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_msu_list.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"
#include <pcre.h>
#include <errno.h>
#include "data_plane_profiling.h"

/** Deserializes data received from remote MSU and enqueues the
 * message payload onto the msu queue.
 *
 * NOTE: If there are substructures in the buffer to be received,
 *       a type-specific deserialize function will have to be
 *       implemented.
 *
 * TODO: I don't think "void *buf" is necessary.
 * @param self MSU to receive data
 * @param msg remote message to be received, containing msg->payload
 * @param buf ???
 * @param bufsize ???
 * @return 0 on success, -1 on error
 */
static int baremetal_deserialize(struct generic_msu *self, intermsu_msg *msg,
                        void *buf, uint16_t bufsize){
    if (self){
        msu_queue_item *recvd =  malloc(sizeof(*recvd));
        if (!(recvd)){
            log_error("Could not allocate msu_queue_item");
            return -1;
        }
#ifdef DATAPLANE_PROFILING
        memset(recvd, 0, sizeof(*recvd));
        recvd->dp_profile_info.dp_seq_count = msg->payload_seq_count;
        recvd->dp_profile_info.dp_id = msg->payload_request_id;
        log_debug("Recieved request id from remote runtime: %lu, seq: %d",recvd->dp_profile_info.dp_id
                , recvd->dp_profile_info.dp_seq_count);
        log_dp_event(self->id, REMOTE_RECV, &recvd->dp_profile_info);
#endif
        struct baremetal_msu_data_payload *baremetal_data = malloc(sizeof(*baremetal_data));
        if (!baremetal_data){
            free(recvd);
            log_error("Could not allocate baremetal_msu_data_payload");
            return -1;
        }
        memcpy(baremetal_data, msg->payload, sizeof(*baremetal_data));

        recvd->buffer_len = sizeof(*baremetal_data);
        recvd->buffer = baremetal_data;
        generic_msu_queue_enqueue(&self->q_in, recvd);
        return 0;
    }
    return -1;
}

/** Mock any delay in MSUs
 * @param generic_msu self
 * @param baremetal_msu_data_payload struct
 * @return 0
 */
static int baremetal_mock_delay(struct generic_msu *self, struct baremetal_msu_data_payload *baremetal_data){
    log_debug("TODO: Can simulate delay here for each baremetal msu");
    usleep(10 * 1000);
    return 0;
}

/** Adds newly accepted socket to array of socket fds in internal state struct for polling
 * @param self msu
 * @param socket fd
 * return 0 on success, -1 on error
 */
static int baremetal_track_socket_poll(struct generic_msu* self, int new_sock_fd){
    struct baremetal_msu_internal_state *in_state = self->internal_state;
/* TODO FIXME: Fix this tracker as we don't do defragmentation of the array
    if(in_state->active_sockets == in_state->total_array_size){
        log_error("TODO: Poll fds socket array full, need to realloc");
        return -1;
    }
*/
    //add the fd at the last index, we make sure that there are no holes in array
    struct pollfd* fds = in_state->fds;
    int i;
    //if(in_state->active_sockets == in_state->total_array_size){
    if(in_state->active_sockets == in_state->total_array_size || 1){ //ALWAYS Happens debug
        for(i = 0;i < in_state->total_array_size; i++){
            if(fds[i].fd < 1){
                //we have a free slot, use this
                fds[i].fd = new_sock_fd;
                fds[i].events = POLLIN;
                log_debug("adding socket: %d at index %d",(*(in_state->fds + i)).fd, i);
                if(in_state->active_sockets < in_state->total_array_size){
                    in_state->active_sockets += 1;
                }
                log_debug("Active sockets num: %d",in_state->active_sockets);
                return 0;
            }
        }
        log_error("TODO: Poll fds socket array full, need to realloc");
        return -1;
    } else {
        fds[in_state->active_sockets].fd = new_sock_fd;
        fds[in_state->active_sockets].events = POLLIN;
        log_debug("adding socket: %d at index %d",(*(in_state->fds + in_state->active_sockets)).fd,
                in_state->active_sockets);
        in_state->active_sockets += 1;
    }
    return 0;
}
/**
 * Clears out socket entry in poll fds tracker for given socket
 * @param self msu
 * @socket fd number to clear
 * @return NULL
 */
static void baremetal_remove_socket_poll(struct generic_msu* self, int socketfd){
    struct baremetal_msu_internal_state *in_state = self->internal_state;
    struct pollfd* fds = in_state->fds;
    int i;
    for(i = 0; i < in_state->total_array_size; i++){
        if(fds[i].fd == socketfd){
            log_debug("Found socket to remove from poll");
            fds[i].fd = -1;
            close(socketfd);
            //TODO rearrange socket in array to make sure there are no holes
            return;
        }
        log_error("Couldn't find socket to remove from poll fd array..shouldn't happen!");
    }
}
/**
 * Recieves data for baremetal MSU
 * @param self baremetal MSU to receive the data
 * @param input_data contains a baremetal_msu_data_payload* in input_data->buffer
 * @return ID of next MSU-type to receive data, or -1 on error
 */
int baremetal_receive(struct generic_msu *self, msu_queue_item *input_data) {
    int ret;
    if (self && input_data) {
        struct baremetal_msu_data_payload *baremetal_data =
            (struct baremetal_msu_data_payload*) (input_data->buffer);
        log_debug("Baremetal receive for msu: %d, msg type: %d",self->id, baremetal_data->type);

        if(baremetal_data->type == NEW_ACCEPTED_CONN){
            //add to list of sockets to poll, should only happen in entry MSU
            if(self->id != 1){
                log_error("NEW_ACCEPTED_CONN type in non-entry msu");
            }
            ret = baremetal_track_socket_poll(self, baremetal_data->socketfd);
            if(ret){
                log_error("Failed to add socket for poll tracking: %d",baremetal_data->socketfd);
                return -1;
            }
            //because this will happen only at entry MSU and the entry msu should poll and recv
            //thus the main listen thread enqueued the accepted socket which the entry msu will
            //now poll on. Here we added that socket to be polled
            return -1;

        } else if(baremetal_data->type == FORWARD){
            baremetal_data->int_data += 1;
            log_debug("FORWARD state, data val: %d",baremetal_data->int_data);
            baremetal_mock_delay(self, baremetal_data);

            //SINK MSU logic
            //Maybe check that if my routing table is empty for next type,
            //that means I am the last sink MSU and should send out the final response?
            struct msu_type *type = &BAREMETAL_MSU_TYPE;
            struct msu_endpoint *dst = get_all_type_msus(self->rt_table, type->type_id);
            if (dst == NULL){
                log_debug("EXIT: No destination endpoint of type %s (%d) for msu %d so an exit!",
                      type->name, type->type_id, self->id);
                //Send call
                char sendbuf[20];
                memset(sendbuf,'\0',sizeof(sendbuf));
                snprintf(sendbuf, 20,"%lu\n",baremetal_data->int_data);
                ret = send(baremetal_data->socketfd, &sendbuf, sizeof(sendbuf),0);
                if(ret == -1){
                    log_error("Failed to send out data on socket: %s",strerror(errno));
                    //baremetal_remove_socket_poll(self, baremetal_data->socketfd);
                    //not here sockets should only be removed via poll mechanism
                } else {
#ifdef DATAPLANE_PROFILING
                    log_dp_event(-1, DEDOS_EXIT, &input_data->dp_profile_info);
#endif
                    log_debug("Sent baremetal response bytes: %d, msg: %s", ret, sendbuf);
                }
                return -1; //since nothing to forward
            }
            return DEDOS_BAREMETAL_MSU_ID;
        }
    }

    if(self && self->id == 1 && !input_data){ //TODO FIXME may create an entry/exit flag in generic_msu
        //for polling sockets on entry msu
        char buffer[BAREMETAL_RECV_BUFFER_SIZE];
        struct baremetal_msu_internal_state *in_state = self->internal_state;
        ret = poll(in_state->fds, in_state->active_sockets, 0);
        if (ret == -1) {
                ;//log_error("erro polling established sockets"); // error occurred in poll()
        } else if (ret == 0) {
                ;//log_debug("Poll Timeout occurred!");
        } else {
            //iterate over the sockets to see which was set and process all
            //log_debug("poll return %d",ret);
            int i;
            unsigned long int recvd_int;
            struct pollfd *pollfd_ptr = in_state->fds;
            for(i = 0; i < in_state->active_sockets; i++){
                if(pollfd_ptr->revents & POLLIN){
                    pollfd_ptr->revents = 0;
                    log_debug("POLLIN on index %d, socket: %d",i,pollfd_ptr->fd);
                    ret = recv(pollfd_ptr->fd, &buffer, BAREMETAL_RECV_BUFFER_SIZE, MSG_WAITALL);
                    if(ret > 0){
                        sscanf(buffer, "%lu", &(recvd_int));
                        log_debug("Received int from client: %lu",recvd_int);

                        struct baremetal_msu_data_payload *data = (struct baremetal_msu_data_payload*)
                                                        malloc(sizeof(struct baremetal_msu_data_payload));
                        data->socketfd = pollfd_ptr->fd;
                        data->type = FORWARD;
                        data->int_data = recvd_int;
                        // get a new queue item and enqueue this within itself because cannot directly
                        // enqueue a queue item to next msu since it's handles by route in msu_receive
                        struct generic_msu_queue_item *new_item_bm = malloc(sizeof(struct generic_msu_queue_item));
                        if(!new_item_bm){
                            log_error("Failed to mallc new bm item");
                            return -1;
                        }
                        memset(new_item_bm,'\0',sizeof(struct generic_msu_queue_item));
#ifdef DATAPLANE_PROFILING
                        //new_item_bm->dp_profile_info.dp_id = get_request_id();

                        /* using seed int data from client as request id to correlate
                         * timing observations from runtime and as observed by client */
                        new_item_bm->dp_profile_info.dp_id = recvd_int;
                        log_dp_event(-1, DEDOS_ENTRY, &new_item_bm->dp_profile_info);
#endif
                        new_item_bm->buffer = data;
                        new_item_bm->buffer_len = sizeof(struct baremetal_msu_data_payload);
                        int baremetal_entry_msu_q_len= generic_msu_queue_enqueue(&self->q_in, new_item_bm); //enqueuing to first bm MSU
                        if(baremetal_entry_msu_q_len < 0){
                            log_error("Failed to enqueue baremetal data to entry msu with id: %d",self->id);
                            free(new_item_bm);
                            free(data);
                        } else {
                            log_debug("Enqueued baremtal request in entry msu, q_len: %d",baremetal_entry_msu_q_len);
                        }

                    } else if(ret == 0){
                        log_debug("Socket closed by peer");
                        close(pollfd_ptr->fd);
                        pollfd_ptr->fd = -1;
                        //TODO rearrange socket in array to make sure there are no holes

                    } else {
                        log_debug("Error in recv: %s",strerror(errno));
                        //TODO rearrange socket in array to make sure there are no holes
                        close(pollfd_ptr->fd);
                        pollfd_ptr->fd = -1;
                    }
                }
                pollfd_ptr++;
            }
            return -1;
            //return DEDOS_BAREMETAL_MSU_ID;
        }
    }
    return -1;
}
/**
 * Init the internal state for entry baremetal MSU to hold active sockets to poll over
 * @param self baremetal MSU to receive the data
 * @return 0 on success, or -1 on error
 */
static int baremetal_init_initial_state(struct generic_msu *self){
    struct baremetal_msu_internal_state *in_state = malloc(sizeof(struct baremetal_msu_internal_state));
    if(!in_state){
        log_error("Failed to malloc internal state");
        return -1;
    }
    in_state->total_array_size = INITIAL_ACTIVE_SOCKETS_SIZE;
    in_state->active_sockets = 0;
    in_state->fds = malloc(in_state->total_array_size * sizeof(struct pollfd));
    if(!in_state->fds){
        log_error("Failded to malloc fds struct array");
        free(in_state);
        return -1;
    }
    memset(in_state->fds, 0x0, sizeof(struct pollfd) * in_state->total_array_size);
    self->internal_state = (struct baremetal_msu_internal_state*) in_state;
    return 0;
}

/**
 * Init the baremetal entry msu if id is 1 and the global pointer in NULL
 * @param self baremetal MSU to receive the data
 * @param create_action information
 * @return 0 on success, or -1 on error
 */
int baremetal_msu_init_entry(struct generic_msu *self,
        struct create_msu_thread_msg_data *create_action)
{
    /* any other internal state that MSU needs to maintain */
    //For routing MSU the internal state will be the chord ring
    if(self->id == 1 && baremetal_entry_msu == NULL){
        int ret = 0;
        baremetal_entry_msu = self;
        ret = baremetal_init_initial_state(self);
        if(ret){
            log_error("Error init baremetal initial state");
            return -1;
        }
        log_debug("Initialized internal state data for entry baremetal msu");
    }
    log_debug("Set baremetal entry msu to be MSU with id: %u",self->id);
    return 0;
}
/**
 * Destroy the internal state if the MSU was an entry MSU
 * @param self msu pointer
 * @return NULL
 */
void baremetal_msu_destroy_entry(struct generic_msu *self){
    if(self->internal_state != NULL){
        log_debug("Freeing internal state for entry baremetal msu");
        struct baremetal_msu_internal_state* in_state = self->internal_state;
        if(in_state->active_sockets != 0){
            log_warn("Active sockets conn in entry msu, but freeing!");
        }
        free(in_state->fds);
        free(in_state);
    }
}

/**
 * All baremetal MSUs contain a reference to this type
 */
const struct msu_type BAREMETAL_MSU_TYPE = {
    .name="baremetal_msu",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_BAREMETAL_MSU_ID,
    .proto_number=MSU_PROTO_NONE,
    .init=baremetal_msu_init_entry,
    .destroy=baremetal_msu_destroy_entry,
    .receive=baremetal_receive,
    .receive_ctrl=NULL,
    .route=round_robin,
    .deserialize=baremetal_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote,
};
