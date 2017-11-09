/**
 * @file msu_msssage.c
 *
 * Messages passed to an MSU
 */
#include "msu_message.h"
#include "logging.h"
#include "communication.h"
#include "local_msu.h"
#include "uthash.h"
#include "runtime_dfg.h"
#include "profiler.h"

#include <stdlib.h>

int init_msu_msg_hdr(struct msu_msg_hdr *hdr, struct msu_msg_key *key) {
    memset(hdr, '\0', sizeof(*hdr));
    hdr->key = *key;
    return 0;
}

unsigned int msu_msg_sender_type(struct msg_provinance *prov) {
    int idx = (prov->path_len - 1) % MAX_PATH_LEN;
    log(LOG_GET_PROVINANCE, "Sender of %p (idx %d) was type %d",
               prov, idx, prov->path[idx].type_id);
    return prov->path[idx].type_id;
}

int set_msg_key(int32_t id, struct msu_msg_key *key) {
    memcpy(&key->key, &id, sizeof(id));
    key->key_len = sizeof(id);
    key->id = id;
    key->group_id = -1;
    return 0;
}

int seed_msg_key(void *seed, size_t seed_size, struct msu_msg_key *key) {
    HASH_VALUE(seed, seed_size, key->id);
    if (seed_size > sizeof(key->key)) {
        log_warn("Key length too large for composite key!");
        seed_size = sizeof(key->key);
    }
    memcpy(&key->key, seed, seed_size);
    key->key_len = seed_size;
    key->group_id = -1;
    return 0;
}

int add_provinance(struct msg_provinance *prov, struct local_msu *sender) {
    if (prov->path_len == 0) {
        prov->origin.type_id = sender->type->id;
        prov->origin.msu_id = sender->id;
        prov->origin.runtime_id = local_runtime_id();
    }
    int idx = prov->path_len % MAX_PATH_LEN;
    log(LOG_ADD_PROVINANCE, "Adding provinance: %d.%d to idx %d, prov %p ",
               sender->type->id, sender->id, idx, prov);
    prov->path[idx].type_id = sender->type->id;
    prov->path[idx].msu_id = sender->id;
    prov->path[idx].runtime_id = local_runtime_id();
    if (prov->path[idx].runtime_id == -1) {
        log_error("Error getting local runtime ip address");
        return -1;
    }
    prov->path_len++;
    log(LOG_ADD_PROVINANCE, "Path len of prov %p is now %d", prov, prov->path_len);
    return 0;
}

int enqueue_msu_msg(struct msg_queue *q, struct msu_msg *data) {
    struct timespec zero = {};
    return schedule_msu_msg(q, data, &zero);
}

int schedule_msu_msg(struct msg_queue *q, struct msu_msg *data, struct timespec *interval) {
    struct dedos_msg *msg = malloc(sizeof(*msg));
    if (msg == NULL) {
        log_perror("Error allocating dedos message");
        return -1;
    }
    msg->type = MSU_MSG;
    msg->data_size = sizeof(*data);
    msg->data = data;

    PROFILE_EVENT(data->hdr, PROF_ENQUEUE);
    if (schedule_msg(q, msg, interval) != 0) {
        log_error("Error MSU message to MSU");
        return -1;
    }
    return 0;
}

struct msu_msg *dequeue_msu_msg(struct msg_queue *q) {
    struct dedos_msg *msg = dequeue_msg(q);
    if (msg == NULL) {
        return NULL;
    }

    if (msg->type != MSU_MSG) {
        log_error("Received non-MSU message on msu queue!");
        return NULL;
    }

    if (msg->data_size != sizeof(struct msu_msg)) {
        log_warn("Data size of dequeued MSU message (%d) does not meet expected (%d)",
                 (int)msg->data_size, (int)sizeof(struct msu_msg));
        return NULL;
    }

    struct msu_msg *msu_msg = msg->data;
    free(msg);
    PROFILE_EVENT(msu_msg->hdr, PROF_DEQUEUE);
    return msu_msg;
}

int read_msu_msg_hdr(int fd, struct msu_msg_hdr *hdr) {
    if (read_payload(fd, sizeof(*hdr), hdr) != 0) {
        log_error("Error reading msu msg header from fd %d", fd);
        return -1;
    }

    return 0;
}

void destroy_msu_msg_and_contents(struct msu_msg *msg) {
    free(msg->data);
    free(msg);
}

struct msu_msg *create_msu_msg(struct msu_msg_hdr *hdr, 
                                     size_t data_size, 
                                     void *data) {
    struct msu_msg *msg = malloc(sizeof(*msg));
    msg->hdr = *hdr;
    msg->data_size = data_size;
    msg->data = data;
    return msg;
}

struct msu_msg *read_msu_msg(struct local_msu *msu, int fd, size_t size) {
    if (size < sizeof(struct msu_msg_hdr)) {
        log_error("Size of incoming message is not big enough to fit header");
        return NULL;
    }
    log(LOG_MSU_MSG_READ, "Reading header from %d", fd);
    struct msu_msg *msg = malloc(sizeof(*msg));
    if (msg == NULL) {
        log_perror("Error allocating MSU message");
        return NULL;
    }
    if (read_msu_msg_hdr(fd, &msg->hdr) != 0) {
        log_error("Error reading msu message header");
        free(msg);
        return NULL;
    }
    PROFILE_EVENT(msg->hdr, PROF_REMOTE_RECV);
    size_t data_size = size - sizeof(msg->hdr);
    void *data = NULL;
    if (data_size > 0) {
        data = malloc(data_size);
        if (data == NULL) {
            log_perror("Error allocating msu msg of size %d", (int)data_size);
            free(msg);
            return NULL;
        }
        log(LOG_MSU_MSG_READ, "Reading payload of size %d from %d", (int)data_size, fd);
        if (read_payload(fd, data_size, data) != 0) {
            log_perror("Error reading msu msg payload of size %d", (int)data_size);
            free(msg);
            free(data);
            return NULL;
        }
        log(LOG_MSU_MSG_DESERIALIZE, "Deserialized MSU message of size %d", (int)data_size);
    }
    msg->data_size = data_size;
    if (msu->type->deserialize) {
        msg->data = msu->type->deserialize(msu, data_size, data, &msg->data_size);
        free(data);
    } else {
        msg->data = data;
    }
    return msg;
}

void *serialize_msu_msg(struct msu_msg *msg, struct msu_type *dst_type, size_t *size_out) {

    if (dst_type->serialize != NULL) {
        void *payload = NULL;
        ssize_t payload_size = dst_type->serialize(dst_type, msg, &payload);
        log(LOG_SERIALIZE,
                   "Serialized message into payload of size %d using %s serialization",
                   (int)payload_size, dst_type->name);
        if (payload_size < 0) {
            log_error("Error running destination type %s's serialize function",
                      dst_type->name);
            return NULL;
        }
        size_t serialized_size = sizeof(struct msu_msg_hdr) + payload_size;
        void *output = malloc(serialized_size);
        if (output == NULL) {
            log_error("Could not allocate serialized MSU msg");
            free(payload);
            return NULL;
        }
        memcpy(output, &msg->hdr, sizeof(msg->hdr));
        memcpy(output + sizeof(msg->hdr), payload, payload_size);

        free(payload);

        *size_out = serialized_size;
        return output;
    } else {
        size_t serialized_size = sizeof(struct msu_msg_hdr) + msg->data_size;
        void *output = malloc(serialized_size);

        if (output == NULL) {
            log_error("Could not allocate serialized MSU msg");
            return NULL;
        }

        memcpy(output, &msg->hdr, sizeof(msg->hdr));
        memcpy(output + sizeof(msg->hdr), msg->data, msg->data_size);

        *size_out = serialized_size;
        return output;
    }
}
