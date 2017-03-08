#ifndef API_H_
#define APIP_H_

#include "dfg.h"

int add_msu(char *data, struct dfg_vertex *new_msu, int runtime_sock);

int update_route(int action, int runtime_sock, int from_msu_id, int from_msu_type,
                 int to_msu_id, int to_msu_type, int to_msu_locality, int to_ip);

int create_worker_thread(int runtime_sock);

#endif
