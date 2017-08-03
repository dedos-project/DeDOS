#include <stdlib.h>
#include "logging.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "modules/webserver/connection-handler.h"

#define VIDEO_MEMORY 999 * 1000 * 1000 * 2
#define AUDIO_MEMORY 1000 * 1000 * 1000 * 1
#define IMAGE_MEMORY 1000 * 1000 * 100
#define TEXT_MEMORY 1000 * 1000 * 10

#ifndef LOG_DB_OPS
#define LOG_DB_OPS 0
#endif

char *db_ip = NULL;
int db_port = -1;
int db_max_load = -1;
const char *DB_QUERY = "REQUEST";
struct sockaddr_in db_addr;

void init_db(char *ip, int port, int max_load) {
    db_ip = ip;
    db_port = port;
    db_max_load = max_load;
    bzero((char*)&db_addr, sizeof(db_addr));
    db_addr.sin_family = AF_INET;
    inet_pton(AF_INET, db_ip, &(db_addr.sin_addr));
    db_addr.sin_port = htons(db_port);

}


void *allocate_db_memory() {
    int malloc_size = VIDEO_MEMORY / 10;
    void *memory = malloc(malloc_size);
    if (memory == NULL) {
        log_perror("Mallocing memory for database");
    }
    return memory;
}

int init_db_socket() {
    int db_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (db_fd < 0) {
        printf("%s", "failure opening socket");
    }
    int optval = 1;
    if (setsockopt(db_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        printf("%s", " failed to set SO_REUSEPORT");
    }
    if (setsockopt(db_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        printf("%s", " failed to set SO_REUSEADDR");
    }
    //add_to_epoll(0, db_fd, 0);
    return db_fd;
}

int connect_to_db(struct db_state *state) {
    if (state->db_fd <= 0) {
        state->db_fd = init_db_socket();
    }
    int rtn = connect(state->db_fd, (struct sockaddr*)&db_addr, sizeof(db_addr)) < 0;
    if (rtn < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS) {
            log_custom(LOG_DB_OPS, "Connection in progress..");
            return 1;
        } else {
            log_perror("Connect to socket failed");
            close(state->db_fd);
            return -1;
        }
    }
    int db_param = rand() % db_max_load;
    sprintf(state->db_req, "%s %d", DB_QUERY, db_param);
    return 0;
}

int send_to_db(struct db_state *state) {
    if (send(state->db_fd, state->db_req, strlen(state->db_req), 0) == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1;
        } else {
            log_perror("Error sending to database on fd %d", state->db_fd);
            return -1;
        }
    }
    return 0;
}

int recv_from_db(struct db_state  *state) {
    // receive response, expecting OK\n
    int res_buf_len = 3;
    char res_buf[res_buf_len];
    memset(res_buf, 0, sizeof(res_buf));
    socklen_t addrlen = sizeof(db_addr);

    int rtn = recvfrom(state->db_fd, res_buf, res_buf_len, 0,
        (void*) &db_addr, &addrlen);

    if (rtn< 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1;
        } else {
            log_perror("Error receiving from database");
            return -1;
        }
    }

    int memSize;
    if (strncmp(res_buf, "00\n", 3) == 0) {
        memSize = (VIDEO_MEMORY);
    } else if (strncmp(res_buf, "01\n", 3) == 0) {
         memSize = (AUDIO_MEMORY);
    } else if (strncmp(res_buf, "02\n", 3) == 0) {
        memSize = (IMAGE_MEMORY);
    } else if (strncmp(res_buf, "03\n", 3) == 0) {
        memSize = (TEXT_MEMORY);
    } else {
        close(state->db_fd);
        log_perror("Received unknown number from database");
        return -1;
    }

    int size = memSize/10;

    char *memory = (char *)state->data;
    int increment = (1<<12);
    for (int i=0; i<size; i+=increment){
        memory[i]++;
    }
    close(state->db_fd);
    state->db_fd = 0;
    return 0;
}

int query_db(struct db_state *state) {
    if (db_ip == NULL) {
        log_error("Database is not initialized");
        return WS_ERROR;

    }

    int rtn;
    switch (state->status) {
        case DB_NO_CONNECTION:
        case DB_CONNECTING:
            rtn = connect_to_db(state);
            if (rtn == -1) {
                state->status = DB_ERR;
                return WS_ERROR;
            } else if (rtn == 1) {
                state->status = DB_CONNECTING;
                return WS_INCOMPLETE_WRITE;
            } else if (rtn == 0) {
                state->status = DB_SEND;
                // Continue on...
            } else {
                log_error("Unknown status %d", rtn);
                state->status = DB_ERR;
                return WS_ERROR;
            }
        case DB_SEND:
            rtn = send_to_db(state);
            if (rtn == -1) {
                state->status = DB_ERR;
                return WS_ERROR;
            } else if (rtn == 1) {
                return WS_INCOMPLETE_WRITE;
            } else if (rtn == 0) {
                state->status = DB_RECV;
                // Continue on...
            } else {
                log_error("Unknown status %d", rtn);
                state->status = DB_ERR;
                return WS_ERROR;
            }
        case DB_RECV:
            rtn = recv_from_db(state);
            if (rtn == -1) {
                state->status = DB_ERR;
                return WS_ERROR;
            } else if (rtn == 1) {
                return WS_INCOMPLETE_READ;
            } else if (rtn == 0) {
                state->status = DB_DONE;
                return WS_COMPLETE;
            } else {
                state->status = DB_ERR;
                log_error("Unknown status %d", rtn);
                return WS_ERROR;
            }
        default:
            log_error("Unhandled state: %d", state->status);
            return WS_ERROR;
    }

}
