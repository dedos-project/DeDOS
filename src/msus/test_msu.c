#include "test_msu.h"
#include "local_msu.h"
#include "socket_msu.h"
#include "rt_stats.h"
#include "msu_type.h"
#include "msu_message.h"
#include "logging.h"
#include <stdlib.h>

#define MAX_TEST_MSG_ID 32
#define MAX_TEST_HOPS 8

pthread_mutex_t id_mutex;
int current_id = 0;

struct test_msu_msg {
    int fd;
    int n_hops;
    int id;
};

static int increment_id() {
    pthread_mutex_lock(&id_mutex);
    int id = current_id++ % MAX_TEST_MSG_ID;
    pthread_mutex_unlock(&id_mutex);
    return id;
}

int receive(struct local_msu *self, struct msu_msg *msg) {
    unsigned int sender_type_id = msu_msg_sender_type(&msg->hdr->provinance);

    struct test_msu_msg *test_msg;
    switch (sender_type_id) {
        case SOCKET_MSU_TYPE_ID:;
            struct socket_msg *sock_msg = msg->data;
            test_msg = malloc(sizeof(*test_msg));
            test_msg->fd = sock_msg->fd;
            char buf[8];
            int rtn = read(sock_msg->fd, buf, sizeof(buf));
            if (rtn <= 0) {
                log_error("Error reading message for test msu hops");
                return -1;
            }
            test_msg->n_hops = atoi(buf);
            test_msg->id = increment_id();
            log_custom(LOG_TEST_MSU, "Received %d-hop message", test_msg->n_hops);
            free(sock_msg);
            record_start_time(MSU_STAT1, test_msg->id);
            break;
        case TEST_MSU_TYPE_ID:
            test_msg = msg->data;
            test_msg->n_hops--;
            record_end_time(MSU_STAT1, test_msg->id);
            break;
        default:
            log_error("Received message from unknown source %d", sender_type_id);
            return -1;
    }

    if (test_msg->n_hops == 0) {
        close(test_msg->fd);
        free(test_msg);
        return 0;
    }

    record_start_time(MSU_STAT1, test_msg->id);
    return call_msu(self, self->type, msg->hdr, sizeof(*test_msg), test_msg);
}

int init_type(struct msu_type *type) {
    for (int i=0; i<MAX_TEST_MSG_ID; i++) {
        int rtn = init_stat_item(MSU_STAT1, i);
        if (rtn < 0) {
            log_error("Error initializing stat item");
        }
    }
    pthread_mutex_init(&id_mutex, NULL);
    return 0;
}



struct msu_type TEST_MSU_TYPE = {
    .name = "Test_Msu",
    .id = TEST_MSU_TYPE_ID,
    .receive = receive,
};

