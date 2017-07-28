#include <stdlib.h>
#include "logging.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>

#define VIDEO_MEMORY 999 * 1000 * 1000 * 2
#define AUDIO_MEMORY 1000 * 1000 * 1000 * 1
#define IMAGE_MEMORY 1000 * 1000 * 100
#define TEXT_MEMORY 1000 * 1000 * 10

char *db_ip = NULL;
int db_port = -1;
int db_max_load = -1;
const char *DB_QUERY = "REQUEST";

void init_db(char *ip, int port, int max_load) {
    db_ip = ip;
    db_port = port;
    db_max_load = max_load;
}


void *allocate_db_memory() {
    int malloc_size = VIDEO_MEMORY / 10;
    void *memory = malloc(malloc_size);
    if (memory == NULL) {
        log_perror("Mallocing memory for database");
    }
    return memory;
}


int query_db(void *data)
{
    if (db_ip == NULL) {
        log_error("Database is not initialized!");
        return -1;
    }
    int param = rand() % db_max_load;
    int sockfd, optval = 1;
    struct sockaddr_in db_addr;

    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("%s", "failure opening socket");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        printf("%s", " failed to set SO_REUSEPORT");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        printf("%s", " failed to set SO_REUSEADDR");
    }

    // build db server's internet address
    bzero((char *)&db_addr, sizeof(db_addr));
    db_addr.sin_family = AF_INET;
    inet_pton(AF_INET, db_ip, &(db_addr.sin_addr));
    db_addr.sin_port = htons(db_port);

    // create a connection
    if (connect(sockfd, (struct sockaddr*)&db_addr, sizeof(db_addr)) < 0) {
        printf("%s", "connect failed");
        close(sockfd);
        return -1;
    }

    // send request
    int req_buf_len = strlen(DB_QUERY) + 1 + 6;
    char *req_buf = (char *) malloc(req_buf_len);
    snprintf(req_buf, req_buf_len + 1, "%s %d", DB_QUERY, param);
    if (send(sockfd, req_buf, strlen(req_buf), 0) == -1) {
        printf("%s", "send failed");
        free(req_buf);
        return -1;
    }

    // receive response, expecting OK\n
    int res_buf_len = 3;
    char res_buf[res_buf_len];
    memset(res_buf, 0, sizeof(res_buf));
    size_t res_len = 0;
    socklen_t addrlen = sizeof(db_addr);    

    res_len = recvfrom(sockfd, res_buf, res_buf_len, 0,
        (void*) &db_addr, &addrlen);

    int memSize;
    if (res_len < 0) {
        free(req_buf);
        return -1;
    } else {
        if (strncmp(res_buf, "00\n", 3) == 0) {
            memSize = (VIDEO_MEMORY);
        } else if (strncmp(res_buf, "01\n", 3) == 0) {
             memSize = (AUDIO_MEMORY);
        } else if (strncmp(res_buf, "02\n", 3) == 0) {
            memSize = (IMAGE_MEMORY);
        } else if (strncmp(res_buf, "03\n", 3) == 0) {
            memSize = (TEXT_MEMORY);
        } else {
            free(req_buf);
            close(sockfd);
            return -1;
        }
    }

    int size = memSize/10;

    char *memory = (char *)data;
    int increment = (1<<12);
    for (int i=0; i<size; i+=increment){
        memory[i]++;
    }
    free(req_buf);
    close(sockfd);
    return 0;
}
