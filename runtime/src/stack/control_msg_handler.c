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

	if (control_msg->msg_type == REQUEST_MESSAGE && control_msg->msg_code == GET_MSU_LIST) {

		unsigned int total_msu_count = msu_tracker_count();
		long unsigned int payload_size = sizeof(struct msu_thread_info)
				* total_msu_count;

		struct msu_thread_info *info = (struct msu_thread_info*) malloc(
				payload_size);
		if (!info) {
			log_error("Failed to malloc for sending msu list %s", "");
			return;
		}
		msu_tracker_get_all_info(info, total_msu_count);

		struct msu_thread_info *tmp;
		log_debug("Sending following MSU placement info to master %s", "");
		int i = 0;
		for (i = 0; i < total_msu_count; i++) {
			log_debug("MSU ID: %d, thread_id: %lu", info[i].msu_id,
					info[i].thread_id);
		}

		struct dedos_control_msg *reply_msg;
		reply_msg = malloc(sizeof(struct dedos_control_msg));
		if (!reply_msg) {
			log_error("failed to allocate memory for sending MSU_LIST to master %s","");
			free(info);
			return;
		}
		reply_msg->msg_type = RESPONSE_MESSAGE;
		reply_msg->msg_code = GET_MSU_LIST;
		reply_msg->header_len = sizeof(struct dedos_control_msg);
		reply_msg->payload_len = payload_size;

		char *sendbuf = malloc(sizeof(struct dedos_control_msg) + payload_size);
		if (!sendbuf) {
			log_error(
					"ERROR: failed to allocate memory for buf to send MSU_LIST to master %s",
					"");
			free(info);
			free(reply_msg);
			return;
		}
		memcpy(sendbuf, reply_msg, sizeof(struct dedos_control_msg));
		memcpy(sendbuf + sizeof(struct dedos_control_msg), info, payload_size);

		struct msu_thread_info* t = (struct msu_thread_info*) (sendbuf
				+ sizeof(struct dedos_control_msg));
		log_debug("Sending following MSU placement info from runtime %s",
				"");
		for (i = 0; i < total_msu_count; i++) {
			log_debug("MSU ID: %d, thread_id: %lu", t[i].msu_id, t[i].thread_id);
		}

		dedos_send_to_master(sendbuf,
				sizeof(struct dedos_control_msg) + payload_size);

		free(sendbuf);
		free(reply_msg);
		free(info);

	}
	else if (control_msg->msg_type == ACTION_MESSAGE && control_msg->msg_code == CREATE_MSU) {

		struct dedos_control_msg_manage_msu *create_msu_msg;
		create_msu_msg =
				(struct dedos_control_msg_manage_msu *) control_msg->payload;
		create_msu_msg->init_data = control_msg->payload
				+ sizeof(struct dedos_control_msg_manage_msu);

		// debug("INSIDE FUNC %s", "After create_msu_msg->init_data");
		// debug("DEBUG: msu_id: %d", create_msu_msg->msu_id);
		// debug("DEBUG: msu_type: %u", create_msu_msg->msu_type);
		// debug("DEBUG: init_data: %s", (char*)create_msu_msg->init_data);

		struct dedos_thread_msg *thread_msg;
		//the corresponding free will be in thread that dequeues the message
		thread_msg = (struct dedos_thread_msg*) malloc(
				sizeof(struct dedos_thread_msg));
		if (!thread_msg) {
			log_error(
					"Failed to malloc thread_msg for CREATE_MSU request %s",
					"");
			return;
		}
		thread_msg->action = control_msg->msg_code;
		thread_msg->action_data = create_msu_msg->msu_id;
		thread_msg->next = NULL;

		struct create_msu_thread_msg_data *create_action = malloc(
				sizeof(struct create_msu_thread_msg_data));
		if (!create_action) {
			log_error(
					"Failed to malloc thread_msg_data for CREATE_MSU request %s",
					"");
			free(thread_msg);
			return;
		}
		thread_msg->data = (struct create_msu_thread_msg_data*) create_action;
		create_action->msu_type = create_msu_msg->msu_type;
		create_action->init_data_len = create_msu_msg->init_data_size;

		create_action->creation_init_data = malloc(
				create_msu_msg->init_data_size);
		if (!create_action->creation_init_data) {
			log_error(
					"Failed to malloc action data for CREATE_MSU request %s",
					"");
			free(create_action);
			free(thread_msg);
			return;
		}

		memcpy(create_action->creation_init_data, create_msu_msg->init_data,
				create_msu_msg->init_data_size);
		thread_msg->buffer_len = sizeof(struct create_msu_thread_msg_data)
				+ create_action->init_data_len;

		int placement_index = -1;
		// placement_index = (create_msu_msg->msu_id % 8);

		if(create_action->init_data_len > 0){
			if(!strncasecmp(create_msu_msg->init_data, "non-blocking", 12)){
				log_debug("Request for creating a non-blocking MSU, it says: %s",create_msu_msg->init_data);
				char *type;
				type = strtok((char*)create_msu_msg->init_data, " ");
				log_debug("Type: %s",type);
        		type = strtok(NULL, "\r\n");
				log_debug("Str thread num: %s",type);
				memset(create_action->creation_init_data, 0, create_action->init_data_len);
				sscanf(type, "%d %s", &placement_index, create_action->creation_init_data);

                /*TODO: handle the case where the request specify a non existing thread within the range of 0-numCPU */
				if(placement_index < 0 || placement_index > total_threads -1) {
					log_error("Placement index must be in range [0,%d] (total_worker_threads), given: %d", total_threads-1, placement_index);
					free(create_action->creation_init_data);
					free(create_action);
					free(thread_msg);
					return;
				}
				log_info("Placement in thread %d", placement_index);
			}

			else if(!strncasecmp(create_action->creation_init_data, "blocking", 8)){
				log_debug("Request for creating a blocking MSU, it says: %s",create_action->creation_init_data);
				//create a new thread and get it's index in the all_threads array
				//update the value of placement_index
				mutex_lock(all_threads_mutex);
				placement_index = on_demand_create_worker_thread(1);
				mutex_unlock(all_threads_mutex);

				if(placement_index < 0){
					log_error("On demand worker thread creation failed %s","");
					free(create_action->creation_init_data);
					free(create_action);
					free(thread_msg);
					return;
				}
				log_info("Placement in newly created thread, since its a blocking MSU that has its own thread: %d", placement_index);
			}
		} else {
			log_error("No init data for msu creation %s","");
			return;
		}

		create_msu_request(&all_threads[placement_index], thread_msg);

		//store the placement info in msu_placements hash structure, though we don't know yet if the creation will
		//succeed. if the creation fails the thread creating the MSU should enqueue a FAIL_CREATE_MSU msg.
		// SEE default case in dedos_create_msu function in generic_msu.c for example.

		msu_tracker_add(thread_msg->action_data, &all_threads[placement_index]);
		msu_tracker_print_all();

	}
	else if (control_msg->msg_type == ACTION_MESSAGE && control_msg->msg_code == DESTROY_MSU) {

		int runtime_sock;
		unsigned int msu_type;
		char *data;

		struct dedos_control_msg_manage_msu *delete_msu_msg;
		delete_msu_msg =
				(struct dedos_control_msg_manage_msu *) control_msg->payload;

		// debug("DEBUG INSIDE FUNC %s", "After create_msu_msg->init_data");
		// debug("DEBUG: msu_id: %d", delete_msu_msg->msu_id);
		// debug("DEBUG: msu_type: %u", delete_msu_msg->msu_type);
		// debug("DEBUG: init_data: %s", delete_msu_msg->init_data);

		struct dedos_thread_msg *thread_msg;
		//the corresponding free will be in thread that dequeues the message
		thread_msg = (struct dedos_thread_msg*) malloc(
				sizeof(struct dedos_thread_msg));
		if (!thread_msg) {
			log_error("Failed to malloc thread_msg for DESTROY_MSU request %s",
					"");
			return;
		}
		thread_msg->action = control_msg->msg_code;
		thread_msg->action_data = delete_msu_msg->msu_id;
		thread_msg->next = NULL;
		int *msu_type_del = (malloc)(sizeof(int));
		if (!msu_type_del) {
			log_error(
					"Failed to malloc msu_type_del for DESTROY_MSU request %s",
					"");
			free(thread_msg);
			return;
		}
		*msu_type_del = msu_type;
		thread_msg->data = (int *) msu_type_del;
		thread_msg->buffer_len = 0;

		struct msu_placement_tracker *msu_tracker;
		msu_tracker = msu_tracker_find(delete_msu_msg->msu_id);
		if(!msu_tracker){
				log_error("Couldn't find the msu_tracker for find the thread with MSU %d", delete_msu_msg->msu_id);
				free(msu_type_del);
				free(thread_msg);
				return;
		}

		int r = get_thread_index(msu_tracker->dedos_thread->tid);
		if(r == -1){
			log_error("Cannot find thread index for thread %u",msu_tracker->dedos_thread->tid);
			free(msu_type_del);
			free(thread_msg);
			return;
		}
		delete_msu_request(&all_threads[r], thread_msg);

		//CAREFUL, the deletion is done by thread but here we remove it from main tracker
		//before that, hoping it will eventually be deleted by the thread that has it
		struct msu_placement_tracker *item;
		item = msu_tracker_find(thread_msg->action_data);
		if (!item) {
			log_error("WARN: Dont have track of MSU to delete, msu_id: %d",
					thread_msg->action_data);
			return;
		}
		msu_tracker_delete(item);
		msu_tracker_print_all();
		//TODO Handle deletion of thread after deleting a blocking MSU
		//If the destroyed MSU was blocking then, store the blocking thread ref in a struct
		//Then in main loop check if there is a any thread in that struct which needs to be destroyed

	}
	else if (!strncasecmp(cmd, "DUMMY_INIT_CONFIG", 17)) {
		/* TODO INIT_CONFIG */
		log_error("TODO: DUMMY_INIT Calling dummy actions to create MSUs %s",
				"");
		printf("DUMMY_INIT_CONFIG!..\n");

	}
	else if (control_msg->msg_type == RESPONSE_MESSAGE
			&& control_msg->msg_code == SET_DEDOS_RUNTIMES_LIST) {

		log_debug("Received Peer IP list %s", "");
		uint32_t *peers = control_msg->payload;
		int count = (control_msg->payload_len) / (sizeof(uint32_t));
		int j = 0;

		if (!pending_runtime_peers) { /* struct not allocated */
			pending_runtime_peers = malloc(sizeof(struct pending_runtimes));
			if (!pending_runtime_peers) {
				log_error(
						"Failed to malloc for pending_runtime_peers struct%s",
						"");
				return;
			}
			pending_runtime_peers->count = 0;
		}

		if (pending_runtime_peers->count == 0) {
			log_debug("DEBUG: No pending connections to runtime %s", "");
			/* no pending stuff, just allocate memorory to hold IPs and done */
			pending_runtime_peers->ips = (uint32_t*) malloc(
					count * sizeof(uint32_t));
			if (!pending_runtime_peers->ips) {
				log_error("Failed to malloc ips array %s", "");
				free(pending_runtime_peers);
				return;
			}
			pending_runtime_peers->count = count;
			while (j < count) {
				pending_runtime_peers->ips[j] = peers[j];
				char ip[40] = { '\0' };
				ipv4_to_string(&ip, pending_runtime_peers->ips[j]);
				log_debug("RCVD PEER IP: %s", ip);
				j++;
			}
		} else {
			log_debug("Pending connections to runtimes: %d",
					pending_runtime_peers->count);
			/* malloc and increase the size */
			int new_count = pending_runtime_peers->count + count;
			pending_runtime_peers->ips = (uint32_t *) realloc(
					pending_runtime_peers->ips, new_count * sizeof(uint32_t));
			j = pending_runtime_peers->count;
			while (j < new_count) {
				pending_runtime_peers->ips[j] = peers[j
						- pending_runtime_peers->count];
				char ip[40] = { '\0' };
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
	}
	else if (control_msg->msg_type == ACTION_MESSAGE
			&& (control_msg->msg_code == MSU_ROUTE_ADD || control_msg->msg_code == MSU_ROUTE_DEL)) {

		unsigned int msu_type;
		char *data;

		struct dedos_control_msg_manage_msu *manage_msu_msg;
		manage_msu_msg = (struct dedos_control_msg_manage_msu *) control_msg->payload;
		manage_msu_msg->init_data = control_msg->payload
				+ sizeof(struct dedos_control_msg_manage_msu);

		// debug("DEBUG: msu_id: %d", manage_msu_msg->msu_id);
		// debug("DEBUG: msu_type: %u", manage_msu_msg->msu_type);
		// debug("DEBUG: init_data_size: %u", manage_msu_msg->init_data_size);
		// debug("DEBUG: init_data: %s", manage_msu_msg->init_data);

		struct dedos_thread_msg *thread_msg;
		//the corresponding free will be in thread that dequeues the message
		thread_msg = (struct dedos_thread_msg*) malloc(
				sizeof(struct dedos_thread_msg));
		if (!thread_msg) {
			log_error(
					"Failed to malloc thread_msg for MSU_ROUTE management request %s",
					"");
			return;
		}
		thread_msg->action = control_msg->msg_code; //MSU_ROUTE_ADD e.g.
		thread_msg->action_data = manage_msu_msg->msu_id;
		thread_msg->next = NULL;

		thread_msg->buffer_len = manage_msu_msg->init_data_size;
		thread_msg->data = malloc(thread_msg->buffer_len);
		memcpy(thread_msg->data, manage_msu_msg->init_data, thread_msg->buffer_len);

		struct msu_placement_tracker *msu_tracker;
		msu_tracker = msu_tracker_find(manage_msu_msg->msu_id);
		if (!msu_tracker) {
			log_error("Couldn't find the msu_tracker for find the thread with MSU %d", manage_msu_msg->msu_id);
			free(thread_msg->data);
			free(thread_msg);
			return;
		}

		int r = get_thread_index(msu_tracker->dedos_thread->tid);
		if (r == -1) {
			log_error("Cannot find thread index for thread %u",
					msu_tracker->dedos_thread->tid);
			free(thread_msg->data);
			free(thread_msg);
			return;
		}
		enqueue_msu_request(&all_threads[r], thread_msg);
	}
	else if(control_msg->msg_type == ACTION_MESSAGE && control_msg->msg_code == CREATE_PINNED_THREAD) {
		log_debug("Pinned thread creation message %s","");
		on_demand_create_worker_thread(0);
	}
	else {
		log_error("ERROR: Unknown control message type %d", control_msg->msg_type);
	}

	return;
}
