#ifndef API_H_
#define APIP_H_

#include "dfg.h"
#include "control_protocol.h"

int add_msu(char *msu_data, int msu_type, int msu_id,
            char *msu_mode, int thread_id, int runtime_sock);

int del_msu( int msu_id, int msu_type, int runtime_sock );
int add_route(int msu_id, int route_id, int runtime_sock);
int del_route(int msu_id, int route_id, int runtime_sock);
int add_endpoint(int msu_id, int route_num, unsigned int range_end, int runtime_sock);
int del_endpoint(int msu_id, int route_id,  int runtime_sock) ;
int mod_endpoint(int msu_id, int route_id, unsigned int range_end, int runtime_sock) ;

int update_route(int action, int runtime_sock, int from_msu_id, int from_msu_type,
                 int to_msu_id, int to_msu_type, enum locality to_msu_locality, int to_ip);

int create_worker_thread(int runtime_sock);

int add_runtime(char *runtime, int runtime_sock);

#endif
