#ifndef DB_OPS_H
#define DB_OPS_H

void init_db(char *ip, int port, int max_load);
void *allocate_db_memory();
int query_db(void *data);
#endif
