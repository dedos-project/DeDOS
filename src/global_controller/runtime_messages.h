#ifndef RUNTIME_MESSAGES_H_
#define RUNTIME_MESSAGES_H_

int send_create_msu_msg(struct dfg_msu *msu);

int send_delete_msu_msg(struct dfg_msu *msu);

int send_create_route_msg(struct dfg_route *route);
int send_delete_route_msg(struct dfg_route *route);
int send_add_route_to_msu_msg(struct dfg_route *route, struct dfg_msu *msu);
int send_add_endpoint_msg(struct dfg_route *route, struct dfg_route_endpoint *endpoint);
int send_del_endpoint_msg(struct dfg_route *route, struct dfg_route_endpoint *endpoint);
int send_mod_endpoint_msg(struct dfg_route *route, struct dfg_route_endpoint *endpoint);
int send_create_thread_msg(struct dfg_thread *thread, struct dfg_runtime *rt);



#endif

