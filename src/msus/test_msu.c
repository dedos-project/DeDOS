#include "test_msu.h"
#include "local_msu.h"
#include "socket_msu.h"
#include "rt_stats.h"
#include "msu_type.h"
#include "msu_message.h"
#include "logging.h"
#include "msu_calls.h"
#include "profiler.h"
#include <stdlib.h>

#define MAX_TEST_MSG_ID 32
#define MAX_TEST_HOPS 8

static pthread_mutex_t id_mutex;
static int current_id = 0;

struct test_msu_msg {
    int fd;
    int id;
    int n_hops;
};

#define DEFAULT_TAG_PROBABILITY 1
#define DEFAULT_N_HOPS 5

struct test_msu_state {
    float tag_probability;
    int n_hops;
};

static int increment_id() {
    pthread_mutex_lock(&id_mutex);
    int id = current_id++ % MAX_TEST_MSG_ID;
    pthread_mutex_unlock(&id_mutex);
    return id;
}

static int receive(struct local_msu *self, struct msu_msg *msg) {
    unsigned int sender_type_id = msu_msg_sender_type(&msg->hdr.provinance);

    struct test_msu_state *state = self->msu_state;
    struct test_msu_msg *test_msg;
    switch (sender_type_id) {
        case SOCKET_MSU_TYPE_ID:;
            struct socket_msg *sock_msg = msg->data;
            test_msg = malloc(sizeof(*test_msg));
            char buf[1024];
            size_t n_read = read(sock_msg->fd, buf, 1024);
            if (n_read < 0) {
                log_error("ERRO RREADING");
            }
            test_msg->fd = sock_msg->fd;
            if ( (float)rand()/(float)RAND_MAX < state->tag_probability) {
                test_msg->id = increment_id();
            } else {
                test_msg->id = -1;
            }
            test_msg->n_hops = state->n_hops;
            free(sock_msg);
            if (test_msg->id >= 0) {
                record_start_time(MSU_STAT1, test_msg->id);
            }
            break;
        case TEST_MSU_TYPE_ID:
            test_msg = msg->data;
            test_msg->n_hops--;
            if (test_msg->id >= 0) {
                record_end_time(MSU_STAT1, test_msg->id);
            }
            break;
        default:
            log_error("Received message from unknown source %d", sender_type_id);
            return -1;
    }

    if (test_msg->n_hops == 0) {
        PROFILE_EVENT(msg->hdr, PROF_DEDOS_EXIT);
        close(test_msg->fd);
        free(test_msg);
        return 0;
    }

    if (test_msg->id >= 0) {
        record_start_time(MSU_STAT1, test_msg->id);
    }
    return call_msu_type(self, self->type, &msg->hdr, sizeof(*test_msg), test_msg);
}

static int init(struct local_msu *self, struct msu_init_data *data) {

    char *init_cmd = data->init_data;
    float tag_probability = DEFAULT_TAG_PROBABILITY;
    int n_hops = DEFAULT_N_HOPS;
    if (init_cmd != NULL && init_cmd[0] != '\0') {
        char *saveptr, *tok;
        if ( (tok = strtok_r(init_cmd, " ,", &saveptr)) == NULL) {
            log_error("Non-null init data with no tag probability! "
                     "Syntax: <tag_prob>, <n_hops>");
            return -1;
        }
        tag_probability = atof(tok);
        if ( (tok = strtok_r(NULL, " ", &saveptr)) == NULL) {
            log_warn("Non-null init data with no n_hops. "
                     "Syntax: <tag_prob>, <n_hops>");
            return -1;
        }
        n_hops = atoi(tok);
        log_info("Initialized TEST MSU:\n\tprobability: %f\n\tn_hops: %d",
                 tag_probability, n_hops);
    }
    struct test_msu_state *state = malloc(sizeof(*state));
    state->tag_probability = tag_probability;
    state->n_hops = n_hops;
    self->msu_state = state;
    return 0;
}

static int init_type(struct msu_type *type) {
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
    .init_type = init_type,
    .init = init,
    .receive = receive,
};

