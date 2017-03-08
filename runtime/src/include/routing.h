#ifndef ROUTING_H_
#define ROUTING_H_

/* routing.h
 *
 * Defines routing table structure for each msu
 * and function declarations to modify the routing table
 *
 * TODO Should also contain structure for per runtime routing table
 */
//#define DEBUG_ROUTING_TABLE

#include "uthash.h"
#include "generic_msu_queue.h"

#define MSU_LOC_SAME_RUNTIME 	1
#define MSU_LOC_REMOTE_RUNTIME 	2

struct msu_endpoint{
    int id;
    unsigned int msu_type;
    uint8_t locality;
    struct generic_msu_queue *next_msu_input_queue; //if local, pointer to next MSUs input q
    uint32_t ipv4; //if remote, IP of remote runtime
    // lookup based access, so other hashtable
    // coz msu can store the id of next msu they sent a
    // flow to for keeping track of flows.
    UT_hash_handle hh;
};

struct msu_routing_table {
    int msu_type;
    struct msu_endpoint *entries;
    UT_hash_handle hh;
};

//separate in case we need more stuff to add/del to each
struct msu_control_add_route {
	int peer_msu_id;
	unsigned int peer_msu_type;
	unsigned int peer_locality;
	unsigned int peer_ipv4; //if locality is remote, then some val else 0
};

struct msu_control_del_route {
	int peer_msu_id;
	unsigned int peer_msu_type;
	unsigned int peer_locality;
	unsigned int peer_ipv4; //if locality is remote, then some val else 0
};

struct msu_control_update {
	int msu_id; //for which MSU the update is for
	unsigned int update_type; //route_add, route_del etc.
	unsigned int payload_len;
	void *payload; //each update type can define its own struct
};

// add an endpoint of type msu_type to msu's routing table rt_table
struct msu_routing_table * add_routing_table_entry(struct msu_routing_table* rt_table, unsigned int msu_type, struct msu_endpoint* entry);
// del given endpoint of type msu_type from msu's routing routing table rt_table
struct msu_routing_table * del_routing_table_entry(struct msu_routing_table* rt_table, unsigned int msu_type, struct msu_endpoint* entry);
// flush all msu_endpoint entries for type msu_type from routing table rt_table
struct msu_routing_table * flush_routing_table_entries(struct msu_routing_table* rt_table, int msu_type);
// delete all entries and key for msu_type
struct msu_routing_table * del_msu_type_entries(struct msu_routing_table *rt_table, int msu_type);
// destroy msu's routing table rt_table
struct msu_routing_table * destroy_routing_table(struct msu_routing_table* rt_table);
// print full routing table rt_table
void print_routing_table(struct msu_routing_table *rt_table);

// return all entries of given msu_type
struct msu_endpoint *get_all_type_msus(struct msu_routing_table *rt_table, unsigned int msu_type);
// get an msu of given id with known type from entries
struct msu_endpoint *get_msu_from_id(struct msu_endpoint *entries, int msu_id);

int ipv4_to_string(char *ipbuf, const uint32_t ip);
int string_to_ipv4(const char *ipstr, uint32_t *ip);

/* Helper functions to call from MSU's to update the routing table */
int is_endpoint_local(struct msu_endpoint *msu_endpoint);
struct msu_routing_table * do_add_route_update(struct msu_routing_table *rt_table, struct msu_control_add_route *add_route_update);
struct msu_routing_table * do_del_route_update(struct msu_routing_table *rt_table, struct msu_control_del_route *del_route_update);

/* Function to call from each msu's control handler for routing messages */
struct msu_routing_table* handle_routing_table_update(struct msu_routing_table *rt_table, struct msu_control_update *update_msg);

void free_msu_control_update(struct msu_control_update *update_msg);

#endif /* ROUTING_H_ */
