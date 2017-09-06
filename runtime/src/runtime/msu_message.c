#include "msu_message.h"
#include "logging.h"
#include "communication.h"
#include <stdlib.h>

int enqueue_msu_msg(struct msg_queue *q, struct msu_msg *data) {
    struct dedos_msg *msg = malloc(sizeof(*msg));
    if (msg == NULL) {
        log_perror("Error allocating dedos message");
        return -1;
    }
    msg->type = MSU_MSG;
    msg->data_size = sizeof(*data);
    msg->data = data;

    if (enqueue_msg(q, msg) != 0) {
        log_error("Error MSU message to MSU");
        return -1;
    }
    return 0;
}

struct msu_msg *dequeue_msu_msg(struct msg_queue *q) {
    struct dedos_msg *msg = dequeue_msg(q);
    if (msg == NULL) {
        log_custom(LOG_FAILED_DEQUEUE_ATTEMPTS, "Could not dequeue from MSU queue");
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
    return msu_msg;
}

static struct msu_msg_hdr *read_msu_msg_hdr(int fd) {
    struct msu_msg_hdr *hdr = malloc(sizeof(*hdr));
    if (hdr == NULL) {
        log_perror("Error allocating msu msg header");
        return NULL;
    }
    if (read_payload(fd, sizeof(*hdr), hdr) != 0) {
        log_error("Error reading msu msg header from fd %d", fd);
        free(hdr);
        return NULL;
    }
    return hdr;
}

static void destroy_msu_msg_hdr(struct msu_msg_hdr *hdr) {
    free(hdr);
}

void destroy_msu_msg(struct msu_msg *msg) {
    free(msg->hdr);
    free(msg->data);
    free(msg);
}

struct msu_msg *read_msu_msg(int fd, size_t size) {
    if (size < sizeof(struct msu_msg_hdr)) {
        log_error("Size of incoming message is not big enough to fit header");
        return NULL;
    }
    struct msu_msg_hdr *hdr = read_msu_msg_hdr(fd);
    if (hdr == NULL) {
        log_perror("Error reading msu msg header");
        return NULL;
    }
    size_t data_size = size - sizeof(*hdr);
    void *data = malloc(data_size);
    if (data == NULL) {
        log_perror("Error allocating msu msg of size %d", (int)data_size);
        destroy_msu_msg_hdr(hdr);
        return NULL;
    }
    if (read_payload(fd, data_size, data) != 0) {
        log_perror("Error reading msu msg payload of size %d", (int)data_size);
        destroy_msu_msg_hdr(hdr);
        free(data);
        return NULL;
    }
    log_custom(LOG_MSU_MSG_DESERIALIZE, "Deserialized MSU message of size %d", data_size);
    struct msu_msg *msg = malloc(sizeof(*msg));
    if (msg == NULL) {
        log_perror("Error allocating msu msg");
        return NULL;
    }
    msg->hdr = hdr;
    msg->data = data;
    return msg;
}

void *serialize_msu_msg(struct msu_msg *msg, struct msu_type *dst_type, size_t *size_out) {

    size_t serialized_size = sizeof(struct msu_msg_hdr) + msg->data_size;
    void *output = malloc(serialized_size);

    if (output == NULL) {
        log_error("Could not allocate serialized MSU msg");
        return NULL;
    }

    memcpy(output, msg->hdr, sizeof(*msg->hdr));

    void *payload = ((char*)output) + sizeof(msg->hdr); 
  
    if (dst_type->serialize != NULL) {
        int rtn = dst_type->serialize(dst_type, msg, payload);
        if (rtn < 0) {
            log_error("Error running destination type %s's serialize function",
                      dst_type->name);
            free(output);
            return NULL;
        }
    } else {
        memcpy(payload, msg->data, msg->data_size);
    }

    *size_out = serialized_size;
    return output;
}
