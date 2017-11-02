#ifndef DB_OPS_H
#define DB_OPS_H

enum db_status {
    DB_NO_CONNECTION,
    DB_CONNECTING,
    DB_SEND,
    DB_RECV,
    DB_DONE,
    DB_ERR
};

struct db_state {
    int db_fd;
    char db_req[16];
    void *data;
    enum db_status status;
};

void init_db(char *ip, int port, int max_load);
void *allocate_db_memory();
void free_db_memory(void *memory);
int query_db(struct db_state *state);
#endif
