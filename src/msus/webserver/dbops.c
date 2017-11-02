#include <stdlib.h>
#include "logging.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "webserver/connection-handler.h"

#ifndef MAX_FILE_SIZE
#define MAX_FILE_SIZE (long)(1 * (1 << 29))
#endif 
#define JUMP_SIZE (1 << 14)

int db_num_files = -1;
struct sockaddr_in db_addr;

struct mock_file {
    int n_chunks;
    int *chunk_size;
    char **data;
};

long allocate_file(struct mock_file *file, long file_size) {
    int n_chunks = file_size / INT_MAX + 1;
    file->n_chunks = n_chunks;
    file->chunk_size = malloc(sizeof(int) * n_chunks);
    long amount_allocated = 0;
    int chunk_size = INT_MAX;
    file->data = malloc(sizeof(char*) * n_chunks);
    for (int i=0; amount_allocated < file_size; i++) {
        if (amount_allocated + chunk_size > file_size) {
            long chunk_tmp = (file_size - amount_allocated);
            if (chunk_tmp > (long)(INT_MAX)) {
                printf("Warning: Chunk size too big!\n");
            }
            chunk_size = (int)chunk_tmp;
        }
        file->chunk_size[i] = chunk_size;
        file->data[i] = malloc(chunk_size);
        amount_allocated += chunk_size;
        memset(file->data[i], 0, chunk_size);
    }
    return amount_allocated;
}

long access_file(struct mock_file *file, long total_size) {
    long accessed = 0;
    for (int chunk = 0; accessed < total_size; chunk++) {
        int chunk_size = file->chunk_size[chunk];
        char *data = file->data[chunk];
        for (int i = 0; i < chunk_size; i += JUMP_SIZE) {
            data[i]++;
        }
        accessed += (long)chunk_size;
    }
    return accessed;
}

void init_db(char *ip, int port, int n_files) {
    db_num_files = n_files;
    bzero((char*)&db_addr, sizeof(db_addr));
    db_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(db_addr.sin_addr));
    db_addr.sin_port = htons(port);
}

void *allocate_db_memory() {
    struct mock_file *file = malloc(sizeof(*file));
    allocate_file(file, MAX_FILE_SIZE);
    return (void*)file;
}

void free_db_memory(void *memory) {
    struct mock_file *file = memory;
    for (int i=0; i <file->n_chunks; i++) {
        free(file->data[i]);
    }
    free(file->chunk_size);
    free(file->data);
    free(file);
}


int init_db_socket() {
    int db_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (db_fd < 0) {
        log_error("failure opening socket");
        return -1;
    }
    int optval = 1;
    if (setsockopt(db_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        log_error("failed to set SO_REUSEPORT");
    }
    if (setsockopt(db_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        log_error("failed to set SO_REUSEADDR");
    }
    //add_to_epoll(0, db_fd, 0);
    return db_fd;
}

int connect_to_db(struct db_state *state) {
    if (state->db_fd <= 0) {
        state->db_fd = init_db_socket();
        if (state->db_fd == -1) {
            return -1;
        }
    }
    int rtn = connect(state->db_fd, (struct sockaddr*)&db_addr, sizeof(db_addr)) < 0;
    if (rtn < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS) {
            log(LOG_DB_OPS, "Connection in progress..");
            return 1;
        } else {
            log_perror("Connect to socket failed");
            close(state->db_fd);
            return -1;
        }
    }
    int db_param = rand() % db_num_files;
    sprintf(state->db_req, "%d", db_param);
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

#define MAX_DB_RCV_LEN 64

int recv_from_db(struct db_state *state) {
    char rcv[MAX_DB_RCV_LEN];
    socklen_t addrlen = sizeof(db_addr);
    int rtn = recvfrom(state->db_fd, rcv, MAX_DB_RCV_LEN, 0, 
                       (struct sockaddr*)&db_addr, &addrlen);

    if (rtn < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 1;
        } else {
            log_perror("Error receiving from database");
            return -1;
        }
    }

    //long file_size = atol(rcv);
    struct mock_file *local_file = state->data;

    access_file(local_file, MAX_FILE_SIZE);
    close(state->db_fd);
    state->db_fd = 0;
    return 0;
}

int query_db(struct db_state *state) {
    if (db_num_files == -1) {
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
