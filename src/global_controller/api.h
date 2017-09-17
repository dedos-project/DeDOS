#ifndef API_H_
#define APIP_H_

#include "dfg.h"

int add_msu(unsigned int msu_id, unsigned int type_id,
            char *init_data_c, char *msu_mode, char *vertex_type,
            unsigned int thread_id, unsigned int runtime_id);

int remove_msu(unsigned int id);

int create_route(unsigned int route_id, unsigned int type_id, unsigned int runtime_id);
int delete_route(unsigned int route_id);
int add_route_to_msu(unsigned int msu_id, unsigned int route_id);
int add_endpoint(unsigned int msu_id, uint32_t key, unsigned int route_id);
int del_endpoint(unsigned int msu_id, unsigned int route_id);
int mod_endpoint(unsigned int msu_id, uint32_t key, unsigned int route_id);
int create_thread(unsigned int thread_id, unsigned int runtime_id, char *mode);
int show_stats(unsigned int msu_id);

#endif
