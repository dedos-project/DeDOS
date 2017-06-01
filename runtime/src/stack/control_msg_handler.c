/* parse all incoming messages coming from global control over
 * tcp conn from the master
 */
#include <unistd.h>
#include <stdlib.h>
#include "control_msg_handler.h"
#include "control_protocol.h"
#include "communication.h"
#include "runtime.h"
#include "logging.h"
#include "control_operations.h"
#include "msu_tracker.h"
#include "serialization.h"

static int request_msu_list(struct dedos_control_msg *control_msg){
    unsigned int total_msu_count = msu_tracker_count();
    struct msu_thread_info *info = malloc(sizeof(*info) * total_msu_count);

    if (!info){
        log_error("Failed to malloc info for GET_MSU_LIST");
        return -1;
    }
    msu_tracker_get_all_info(info, total_msu_count);

    log_debug("Sending following MSU placement info to master:");
    for (int i=0; i<total_msu_count; i++){
        log_debug("MSU ID: %d, thread ID: %lu",
                  info[i].msu_id, info[i].thread_id);
    }

    struct dedos_control_msg *reply_msg = malloc(sizeof(*reply_msg));
    if (!reply_msg){
        log_error("Failed to allocate reply msg for GET_MSU_LIST");
        free(info);
        return -1;
    }

    reply_msg->msg_type = RESPONSE_MESSAGE;
    reply_msg->msg_code = GET_MSU_LIST;
    reply_msg->header_len = sizeof(*reply_msg);
    reply_msg->payload_len = sizeof(*info) * total_msu_count;
    reply_msg->payload = info;

    int rtn = send_control_msg(controller_sock, reply_msg);

    free(reply_msg);
    free(info);
    return 0;
}

static int action_create_msu(struct dedos_control_msg *control_msg){
    struct manage_msu_control_payload *create_msu_msg = control_msg->payload;
    create_msu_msg->init_data = (void*)(create_msu_msg+1);

    struct dedos_thread_msg *thread_msg = malloc(sizeof(*thread_msg));
    if (!thread_msg){
        log_error("Failed to allocate thread_msg for CREATE_MSU");
        return -1;
    }
    thread_msg->action = control_msg->msg_code;
    thread_msg->action_data = create_msu_msg->msu_id;
    thread_msg->next = NULL;

    struct create_msu_thread_data *create_action =
            malloc(sizeof(*create_action));

    if (!create_action){
        log_error("Failed to allocate thread_data for CREATE_MSU");
        free(thread_msg);
        return -1;
    }

    thread_msg->data = create_action;
    create_action->msu_id = create_msu_msg->msu_id;
    create_action->msu_type = create_msu_msg->msu_type;
    create_action->init_data_len = create_msu_msg->init_data_size;
    create_action->init_data = malloc(create_msu_msg->init_data_size);
    if (!create_action->init_data){
        log_error("Failed to allocate action data for CREATE_MSU");
        free(create_action);
        free(thread_msg);
        return -1;
    }

    memcpy(create_action->init_data, create_msu_msg->init_data,
            create_msu_msg->init_data_size);
    thread_msg->buffer_len = sizeof(*thread_msg) + create_action->init_data_len;

    int placement_index = -1;

    if (create_action->init_data_len > 0){
        if (strncasecmp(create_msu_msg->init_data, "non-blocking", 12) == 0){
            log_debug("Request for creation non-blocking MSU");
            char *type;
            type = strtok((char*)create_msu_msg->init_data, " ");
            log_debug("Type: %s", type);
            type = strtok(NULL, "\r\n");
            log_debug("Str thread num: %s", type);
            memset(create_action->init_data, 0, create_action->init_data_len);
            sscanf(type, "%d %s", &placement_index, (char*)create_action->init_data);

            if (placement_index < 0 || placement_index > total_threads - 1){
                log_error("Placement index must be in range [0,%d] (total_worker_threads), given: %d",
                           total_threads-1, placement_index);
                free(create_action->init_data);
                free(create_action);
                free(thread_msg);
                return -1;
            }
            log_info("Placement in thread %d", placement_index);
        } else if (strncasecmp(create_action->init_data, "blocking", 8) == 0){
            log_debug("Request for creation of blocking MSU");

            mutex_lock(all_threads_mutex);
            placement_index = on_demand_create_worker_thread(1);
            mutex_unlock(all_threads_mutex);

            if (placement_index < 0){
                log_error("On demand worker thread creation failed");
                free(create_action->init_data);
                free(create_action);
                free(thread_msg);
                return -1;
            }
            log_info("Placement in newly created thread, since its a blocking MSU that has its own thread: %d", placement_index);
        } else {
            log_error("Blocking type for new MSU not specified (%s)", create_msu_msg->init_data);
            return -1;
        }
    } else {
        log_error("No initial data for MSU creation");
        return -1;
    }

    create_msu_request(&all_threads[placement_index], thread_msg);
    //store the placement info in msu_placements hash structure, though we don't know yet if the creation will
    //succeed. if the creation fails the thread creating the MSU should enqueue a FAIL_CREATE_MSU msg.
    // SEE default case in dedos_create_msu function in generic_msu.c for example.

    msu_tracker_add(thread_msg->action_data, &all_threads[placement_index]);
    msu_tracker_print_all();
    return 0;
}

static int action_destroy_msu(struct dedos_control_msg *control_msg){
    struct manage_msu_control_payload *delete_msu_msg =
            (struct manage_msu_control_payload *)control_msg->payload;

    struct dedos_thread_msg *thread_msg = malloc(sizeof(*thread_msg));
    if (!thread_msg){
        log_error("Failed to allocate thread_msg for DESTROY_MSU");
        return -1;
    }

    thread_msg->action = control_msg->msg_code;
    thread_msg->action_data = delete_msu_msg->msu_id;
    thread_msg->next = NULL;
    thread_msg->data = 0;
    thread_msg->buffer_len = 0;

    struct msu_placement_tracker *msu_tracker =
            msu_tracker_find(delete_msu_msg->msu_id);

    if (!msu_tracker){
        log_error("Couldn't find msu tracker for MSU id %d", delete_msu_msg->msu_id);
        free(thread_msg);
        return -1;
    }

    int r = get_thread_index(msu_tracker->dedos_thread->tid);
    if (r == -1){
        log_error("Cannot find thread index for thread %u", (unsigned)msu_tracker->dedos_thread->tid);
        free(thread_msg);
        return -1;
    }
    delete_msu_request(&all_threads[r], thread_msg);

    //CAREFUL, the deletion is done by thread but here we remove it from main tracker
    //before that, hoping it will eventually be deleted by the thread that has it
    struct msu_placement_tracker *item;
    item = msu_tracker_find(thread_msg->action_data);
    if (!item) {
        log_error("WARN: Dont have track of MSU to delete, msu_id: %d",
                thread_msg->action_data);
        return -1;
    }
    msu_tracker_delete(item);
    msu_tracker_print_all();
    //TODO Handle deletion of thread after deleting a blocking MSU
    //If the destroyed MSU was blocking then, store the blocking thread ref in a struct
    //Then in main loop check if there is a any thread in that struct which needs to be destroyed
    return 0;
}

static int response_set_dedos_runtimes_list(struct dedos_control_msg *control_msg){
    log_debug("Received Peer IP list");
    uint32_t *peers = control_msg->payload;
    int count = (control_msg->payload_len) / sizeof(uint32_t);

    if (!pending_runtime_peers) { /* struct not allocated */
        pending_runtime_peers = malloc(sizeof(struct pending_runtimes));
        if (!pending_runtime_peers) {
            log_error("Failed to alocate pending_runtime_peers struct");
            return -1;
        }
        pending_runtime_peers->count = 0;
    }

    if (pending_runtime_peers->count == 0){
        log_debug("No pending connections to runtime");
			/* no pending stuff, just allocate memorory to hold IPs and done */
        pending_runtime_peers->ips = malloc(count * sizeof(uint32_t));
        if (!pending_runtime_peers->ips) {
            log_error("Failed to allocate ips array");
            free(pending_runtime_peers);
            return -1;
        }
        pending_runtime_peers->count = count;
        for (int j=0; j<count; j++){
            pending_runtime_peers->ips[j] = peers[j];
            char ip[40] = {'\0'};
            ipv4_to_string(&ip, pending_runtime_peers->ips[j]);
            log_debug("Received peer IP: %s", ip);
            j++;
        }
    } else {
        log_debug("Pending connections to runtimes: %d", pending_runtime_peers->count);
        // Malloc and increase the size
        int new_count = pending_runtime_peers->count + count;
        pending_runtime_peers->ips = realloc(pending_runtime_peers->ips,
                                             new_count * sizeof(uint32_t));
        for (int j=pending_runtime_peers->count; j<new_count; j++){
            pending_runtime_peers->ips[j] = peers[j - pending_runtime_peers->count];
            char ip[4] = {'\0'};
            ipv4_to_string(&ip, pending_runtime_peers->ips[j]);
            log_debug("IP: %s", ip);
            j++;
        }
        pending_runtime_peers->count = new_count;

        log_debug("New list of pending runtime ips: %s", "");
        int k = pending_runtime_peers->count;
        int z = 0;
        while (z < k) {
            char ip[40] = { '\0' };
            ipv4_to_string(&ip, pending_runtime_peers->ips[z]);
            log_debug("IP: %s", ip);
            z++;
        }
        //connections will be made inside the dedos_main_thread_loop()
    }
    return 0;
}

static int action_modify_msu_routes(struct dedos_control_msg *control_msg){
    struct manage_msu_control_payload *manage_msu_msg =
            (struct manage_route_control_payload *)control_msg->payload;

    struct dedos_thread_msg *thread_msg = malloc(sizeof(*thread_msg));
    if (!thread_msg){
        log_error("Failed to allocate thread_msg for modifying MSU route");
        return -1;
    }
    thread_msg->action = control_msg->msg_code;
    thread_msg->action_data = manage_msu_msg->msu_id;
    thread_msg->next = NULL;

    struct msu_action_thread_data *route_thread_msg = malloc(sizeof(*route_thread_msg));
    if (!route_thread_msg){
        log_error("Failed to allocate routing thread msg");
        return -1;
    }
    route_thread_msg->msu_id = manage_msu_msg->msu_id;
    route_thread_msg->action = control_msg->msg_code;
    route_thread_msg->route_id = manage_msu_msg->route_id;
    thread_msg->buffer_len = sizeof(*route_thread_msg);
    thread_msg->data = route_thread_msg;

    struct msu_placement_tracker *msu_tracker =
            msu_tracker_find(manage_msu_msg->msu_id);

    if (!msu_tracker){
        log_error("Couldn't find msu_tracker for MSU %d", manage_msu_msg->msu_id);
        free(thread_msg->data);
        free(thread_msg);
        return -1;
    }

    int r = get_thread_index(msu_tracker->dedos_thread->tid);
    if (r == -1) {
        log_error("Cannot find thread index for thread %u", msu_tracker->dedos_thread->tid);
        free(thread_msg->data);
        free(thread_msg);
        return -1;
    }
    enqueue_msu_request(&all_threads[r], thread_msg);
    return 0;
}

static int action_modify_route_msus(struct dedos_control_msg *control_msg){
    struct manage_route_control_payload *manage_route_msg =
            (struct manage_route_control_payload *)control_msg->payload;
    int route_id = manage_route_msg->route_id;
    int msu_id = manage_route_msg->msu_id;
    int msu_type_id = manage_route_msg->msu_type_id;
    uint32_t key_range = manage_route_msg->key_range;
    int rtn;
    switch (control_msg->msg_code){
        case ROUTE_ADD_ENDPOINT:;
            struct msu_endpoint endpoint;
            endpoint.type_id = msu_type_id;
            endpoint.id = msu_id;
            endpoint.locality = manage_route_msg->locality;

            if (endpoint.locality == MSU_IS_LOCAL){
                endpoint.msu_queue = get_msu_queue(msu_id, msu_type_id);
                if (endpoint.msu_queue == NULL){
                    log_error("Could not find msu queue! Not adding endpoint!");
                    return -1;
                }
            } else {
                endpoint.ipv4 = manage_route_msg->ipv4;
            }

            rtn = add_route_endpoint(route_id, &endpoint, key_range);
            if (rtn < 0){
                log_error("Failed to add endpoint %d to route %d",
                          msu_id, route_id);
                return -1;
            }
            return 0;
        case ROUTE_DEL_ENDPOINT:
            rtn = remove_route_destination(route_id, msu_id);
            if (rtn < 0){
                log_error("Failed to remove destination %d from route %d",
                          msu_id, route_id);
                return -1;
            }
            return 0;
        case ROUTE_MOD_ENDPOINT:
            rtn = modify_route_key_range(route_id, msu_id, key_range);
            if (rtn < 0){
                log_error("Failed to change msu %d key range to %d in route %d",
                          msu_id, key_range, route_id);
                return -1;
            }
            return 0;
        default:
            log_error("Got unknown action type (%d) when modifying routes",
                      control_msg->msg_code);
            return -1;
    }
}

static int cmd_dummy_config(struct dedos_control_msg *control_msg){
    log_error("TODO: DUMMY_INIT Calling dummy actions to create MSUs");
    return -1;
}

static int action_create_pinned_thread(struct dedos_control_msg *control_msg){
    log_debug("Pinned thread creation message received");
    on_demand_create_worker_thread(0);
    return 0;
}

static int parse_request_msg(struct dedos_control_msg *control_msg){
    switch (control_msg->msg_code){
        case GET_MSU_LIST:
            return request_msu_list(control_msg);
        default:
            log_error("Unknown request message code received: %d", control_msg->msg_code);
            return -1;
    }
}

static int parse_action_msg(struct dedos_control_msg *control_msg){
    switch (control_msg->msg_code){
        case CREATE_MSU:
            return action_create_msu(control_msg);
        case DESTROY_MSU:
            return action_destroy_msu(control_msg);
        case ADD_ROUTE_TO_MSU:
        case DEL_ROUTE_FROM_MSU:
            return action_modify_msu_routes(control_msg);
        case ROUTE_ADD_ENDPOINT:
        case ROUTE_DEL_ENDPOINT:
        case ROUTE_MOD_ENDPOINT:
            return action_modify_route_msus(control_msg);
        case CREATE_PINNED_THREAD:
            return action_create_pinned_thread(control_msg);
        default:
            log_error("Got unknown action message code %d", control_msg->msg_code);
            return -1;
    }
}

static int parse_response_msg(struct dedos_control_msg *control_msg){
    switch (control_msg->msg_code){
        case SET_DEDOS_RUNTIMES_LIST:
            return response_set_dedos_runtimes_list(control_msg);
        default:
            log_error("Got unknown response message code %d", control_msg->msg_code);
            return -1;
    }
}

// TODO refactor this into case statements
void parse_cmd_action(char *cmd) {
	int count = 0;
	char *buf;
	unsigned long *peer_ips = NULL;
	struct dedos_control_msg *control_msg;

	control_msg = (struct dedos_control_msg*) cmd;
	if (control_msg->payload_len) {
		control_msg->payload = cmd + control_msg->header_len;
	}

	size_t ln = strlen(cmd) - 1;
	if (*cmd && cmd[ln] == '\n') {
		cmd[ln] = '\0';
	}

    if (strncasecmp(cmd, "DUMMY_INIT_CONFIG", 17) == 0){
        cmd_dummy_config(control_msg);
        return;
    }

    switch (control_msg->msg_type){
        case REQUEST_MESSAGE:
            if (parse_request_msg(control_msg) < 0){
                log_error("Error parsing REQUEST_MESSAGE");
            }
            return;
        case ACTION_MESSAGE:
            if (parse_action_msg(control_msg) < 0){
                log_error("Error parsing ACTION_MESSAGE");
            }
            return;
        case RESPONSE_MESSAGE:
            if (parse_response_msg(control_msg) < 0){
                log_error("Error parsing RESPONSE_MESSAGE");
            }
            return;
		default:
            log_error("ERROR: Unknown control message type %d", control_msg->msg_type);
	}
}
