/*
 * routing.c
 *
 * defines functions for manipulation of msu routing table
 *
 */
#include <string.h>
#include "routing.h"
#include "msu_tracker.h"
#include "dedos_msu_pool.h"
#include "logging.h"

static inline int is_digit(char c)
{
    if (c < '0' || c > '9')
        return 0;

    return 1;
}

#ifdef PICO_BIGENDIAN

static inline uint32_t long_from(void *_p)
{
    unsigned char *p = (unsigned char *)_p;
    uint32_t r, p0, p1, p2, p3;
    p0 = p[0];
    p1 = p[1];
    p2 = p[2];
    p3 = p[3];
    r = (p0 << 24) + (p1 << 16) + (p2 << 8) + p3;
    return r;
}

#else

static inline uint32_t long_from(void *_p){
    unsigned char *p = (unsigned char *)_p;
    uint32_t r, _p0, _p1, _p2, _p3;
    _p0 = p[0];
    _p1 = p[1];
    _p2 = p[2];
    _p3 = p[3];
    r = (_p3 << 24) + (_p2 << 16) + (_p1 << 8) + _p0;
    return r;
}
#endif

int ipv4_to_string(char *ipbuf, const uint32_t ip)
{
    const unsigned char *addr = (const unsigned char *) &ip;
    int i;

    if (!ipbuf) {
        return -1;
    }

    for (i = 0; i < 4; i++) {
        if (addr[i] > 99) {
            *ipbuf++ = (char) ('0' + (addr[i] / 100));
            *ipbuf++ = (char) ('0' + ((addr[i] % 100) / 10));
            *ipbuf++ = (char) ('0' + ((addr[i] % 100) % 10));
        } else if (addr[i] > 9) {
            *ipbuf++ = (char) ('0' + (addr[i] / 10));
            *ipbuf++ = (char) ('0' + (addr[i] % 10));
        } else {
            *ipbuf++ = (char) ('0' + addr[i]);
        }

        if (i < 3)
            *ipbuf++ = '.';
    }
    *ipbuf = '\0';

    return 0;
}

int string_to_ipv4(const char *ipstr, uint32_t *ip)
{
    unsigned char buf[4] = { 0 };
    int cnt = 0;
    char p;

    if (!ipstr || !ip) {
        return -1;
    }
    while ((p = *ipstr++) != 0 && cnt < 4) {
        if (is_digit(p)) {
            buf[cnt] = (uint8_t) ((10 * buf[cnt]) + (p - '0'));
        } else if (p == '.') {
            cnt++;
        } else {
            return -1;
        }
    }
    /* Handle short notation */
    if (cnt == 1) {
        buf[3] = buf[1];
        buf[1] = 0;
        buf[2] = 0;
    } else if (cnt == 2) {
        buf[3] = buf[2];
        buf[2] = 0;
    } else if (cnt != 3) {
        /* String could not be parsed, return error */
        return -1;
    }

    *ip = long_from(buf);

    return 0;
}

static struct msu_endpoint* add_endpoint_entry(struct msu_endpoint *entries, int id,
        int msu_type, uint8_t locality, struct generic_msu_queue *next_msu_q_ptr, uint32_t ipv4)
{
    //assumption is id's are unique, so if an add call with existing id is given, it will
    //update the ip of the msu_endpoint

    struct msu_endpoint *entry;
    HASH_FIND_INT(entries, &id, entry); //msu_type already in a hash?
    if (entry == NULL) { //See if msu with id already exists, is so then update the entry
        entry = (struct msu_endpoint*) malloc(sizeof(struct msu_endpoint));
        entry->id = id;
        HASH_ADD_INT(entries, id, entry);
    }
    entry->msu_type = msu_type;
    entry->locality = locality;
    entry->next_msu_input_queue = next_msu_q_ptr;
    entry->ipv4= ipv4;
    return entries;
}

static struct msu_endpoint* del_endpoint_entry(struct msu_endpoint *entries,
        int id)
{
    struct msu_endpoint *endpoint;
    HASH_FIND_INT(entries, &id, endpoint); //msu_type already in a hash?
    if(endpoint != NULL){
        HASH_DEL(entries, endpoint);
        free(endpoint);
    }
    return entries;
}

static struct msu_endpoint* del_all_endpoint_entries(struct msu_endpoint *entries)
{
    struct msu_endpoint *cur, *tmp;
    HASH_ITER(hh, entries, cur, tmp)
    {
        printf("HASH_ITER Loop\n");
        HASH_DEL(entries, cur);
        free(cur);
    }
    return entries;
}

static void print_endpoints(struct msu_endpoint *entries)
{
    struct msu_endpoint *tmp;
    char ip[30];
    for (tmp = entries; tmp != NULL; tmp =
            (struct msu_endpoint*) (tmp->hh.next)) {
        memset(ip, 0, 30);
        ipv4_to_string(ip, tmp->ipv4);
        log_debug("\tMSU_TYPE: %d MSU_ID: %d: Locality: %u next_queue at : %p IP: %s", tmp->msu_type, tmp->id,
        		tmp->locality, tmp->next_msu_input_queue, ip);
    }
}

struct msu_routing_table * add_routing_table_entry(struct msu_routing_table* rt_table, unsigned int msu_type,
        struct msu_endpoint* entry)
{
    // add an endpoint of type msu_type to msu's routing table rt_table
    struct msu_routing_table *tmp;
    if (entry != NULL && msu_type != entry->msu_type) {
        log_error("msu_type mismatch..not adding routing table entry%s","");
        return rt_table;
    }
    HASH_FIND_INT(rt_table, &msu_type, tmp); //msu_type already in a hash?
    if (tmp == NULL) {
        tmp = (struct msu_routing_table*) malloc(
                sizeof(struct msu_routing_table));
        tmp->msu_type = msu_type;
        HASH_ADD_INT(rt_table, msu_type, tmp);
        tmp->entries = NULL; //initialize hashtable for msu endpoints
    }

    tmp->entries = add_endpoint_entry(tmp->entries, entry->id, msu_type, entry->locality, entry->next_msu_input_queue, entry->ipv4);
    return rt_table;
}

struct msu_routing_table * del_routing_table_entry(struct msu_routing_table* rt_table, unsigned int msu_type,
        struct msu_endpoint* entry)
{
    // del given endpoint of type msu_type from msu's routing routing table rt_table
    struct msu_routing_table *tmp;
    if (msu_type != entry->msu_type) {
        log_error("msu_type mismatch..not deleting routing table entry%s","");
        return rt_table;
    }
    HASH_FIND_INT(rt_table, &msu_type, tmp);
    if (tmp != NULL) {
        log_debug("%s","Found msu_type key...");
        log_debug("For type: %d, before del_endpoint_entry tmp->entries = %p\n", msu_type, tmp->entries);
        tmp->entries = del_endpoint_entry(tmp->entries, entry->id);
    } else {
        log_warn("No entry in routing table for msu_type: %u", msu_type);
    }
    return rt_table;
}

struct msu_routing_table * flush_routing_table_entries(struct msu_routing_table* rt_table,
        int msu_type)
{
    // delete all msu_endpoint entries for type msu_type from routing table rt_table
    struct msu_routing_table *tmp;
    HASH_FIND_INT(rt_table, &msu_type, tmp);
    if (tmp != NULL) {
        tmp->entries = del_all_endpoint_entries(tmp->entries);
    } else {
        log_warn("No entry in routing table for msu_type: %u", msu_type);
    }
    return rt_table;
}

struct msu_routing_table * del_msu_type_entries(struct msu_routing_table *rt_table, int msu_type)
{
    // delete all entries and key for msu_type
    flush_routing_table_entries(rt_table, msu_type);
    struct msu_routing_table *tmp;
    HASH_FIND_INT(rt_table, &msu_type, tmp);
    if (tmp != NULL) {
        HASH_DEL(rt_table, tmp);
        free(tmp);
    } else {
        log_warn("No entry in routing table for msu_type: %u", msu_type);
    }
    return rt_table;
}

struct msu_routing_table * destroy_routing_table(struct msu_routing_table* rt_table)
{
    struct msu_routing_table *cur, *tmp;
    HASH_ITER(hh, rt_table, cur, tmp)
    {
        rt_table = flush_routing_table_entries(rt_table, cur->msu_type);
        HASH_DEL(rt_table, cur);
        free(cur);
        print_routing_table(rt_table);
    }
    print_routing_table(rt_table);
    return rt_table;
}

void print_routing_table(struct msu_routing_table *rt_table)
{
	log_info("Routing table------>%s","");
    struct msu_routing_table *tmp;
    for (tmp = rt_table; tmp != NULL;
            tmp = (struct msu_routing_table*) (tmp->hh.next)) {
        log_info("MSU_TYPE: %d", tmp->msu_type);
        // log_info("tmp->entries = %p", tmp->entries);
        if(tmp->entries){
            print_endpoints(tmp->entries);
        }
    }
}


struct msu_endpoint *get_all_type_msus(struct msu_routing_table *rt_table, unsigned int msu_type)
{
    struct msu_routing_table *tmp;
    HASH_FIND_INT(rt_table, &msu_type, tmp);
    if (tmp != NULL) {
       return tmp->entries;
    } else {
        log_warn("No entry in routing table for msu_type: %u", msu_type);
        return NULL;
    }
}

struct msu_endpoint *get_msu_from_id(struct msu_endpoint *entries, int msu_id){
    struct msu_endpoint *tmp;
    HASH_FIND_INT(entries, &msu_id, tmp);
    if (tmp != NULL) {
       return tmp;
    } else {
        log_warn("No entry in routing table for msu_id: %d", msu_id);
        return NULL;
    }
}

/* Helper functions to call from MSU's to update the routing table */
static struct generic_msu_queue *get_input_queue_ptr(int msu_id, unsigned int msu_type){
	struct msu_placement_tracker *tracker;
	struct generic_msu *next_msu;
	tracker = NULL;
	next_msu = NULL;

	//first get dedos_thread_ptr from msu_placements
	tracker = msu_tracker_find(msu_id);
	if(!tracker){
		log_error("Couldn't find the msu_tracker for getting to thread with MSU %d", msu_id);
		return NULL;
	}
	//then ask the thread's msu pool to get a pointer to the msu
	next_msu = dedos_msu_pool_find(tracker->dedos_thread->msu_pool, msu_id);
	if(!next_msu){
       log_error("Failed to get ptr to next MSU from msu_pool %s","");
		return NULL;
	}
	if(next_msu->type->type_id != msu_type){
		log_error("Found msu with matching IDs %d=%d but mismatching types %u != %u",
				msu_id, next_msu->id, msu_type, next_msu->type->type_id);
		return NULL;
	}
	return &next_msu->q_in;
}

int is_endpoint_local(struct msu_endpoint *msu_endpoint){
    if(msu_endpoint->locality == MSU_LOC_SAME_RUNTIME){
    	return 0;
    }
    return 1;
}

struct msu_routing_table* do_add_route_update(struct msu_routing_table *rt_table, struct msu_control_add_route *add_route_update){

	//create and fill msu_endpoint struct
    struct msu_endpoint add_entry;
    add_entry.id = add_route_update->peer_msu_id;
    add_entry.msu_type = add_route_update->peer_msu_type;
    add_entry.locality = add_route_update->peer_locality;
    add_entry.next_msu_input_queue = NULL;
	add_entry.ipv4 = 0;

    //now based on locality either find the ref to queue to next msu or just populate the ipv4 addr
    if(add_entry.locality == MSU_LOC_SAME_RUNTIME){
    	log_debug("Same locality for next msu, finding queue ptr to it %s","");
    	add_entry.next_msu_input_queue = get_input_queue_ptr(add_entry.id, add_entry.msu_type);
    	if(add_entry.next_msu_input_queue == NULL){
    		log_error("Unable to get next msu queue ptr ref %s","");
    		goto err;
    	}
    }
    else if(add_entry.locality == MSU_LOC_REMOTE_RUNTIME){
    	log_debug("Remote locality for next msu, adding remote ip %s","");
    	if(add_route_update->peer_ipv4 == 0){
    		log_error("peer ip is 0 for remote locality! %s","");
    		goto err;
    	}
    	add_entry.ipv4 = add_route_update->peer_ipv4;
    }
    else{
    	log_error("Unknown locality %u",add_entry.locality);
    	goto err;
    }

    rt_table = add_routing_table_entry(rt_table, add_route_update->peer_msu_type, &add_entry);
    log_debug("Added routing table entry for peer msu: %d",add_entry.id);
    print_routing_table(rt_table);

	return rt_table;
err:
	log_error("Failed to do route_update %s","");
	return NULL;
}

struct msu_routing_table* do_del_route_update(struct msu_routing_table *rt_table, struct msu_control_del_route *del_route_update){

	//create and fill msu_endpoint struct
    struct msu_endpoint del_entry;
    del_entry.id = del_route_update->peer_msu_id;
    del_entry.msu_type = del_route_update->peer_msu_type;
    del_entry.locality = del_route_update->peer_locality;
    del_entry.next_msu_input_queue = NULL;
    del_entry.ipv4= 0;

    //now based on locality either find the ref to queue to next msu or just populate the ipv4 addr
    if(del_entry.locality == MSU_LOC_SAME_RUNTIME){
    	log_debug("Same locality for next msu, finding queue ptr to it %s","");
    	del_entry.next_msu_input_queue = get_input_queue_ptr(del_entry.id, del_entry.msu_type);
    	if(del_entry.next_msu_input_queue == NULL){
    		log_error("Unable to get next msu queue ptr ref %s","");
    		goto err;
    	}
    }
    else if(del_entry.locality == MSU_LOC_REMOTE_RUNTIME){
    	log_debug("Remote locality for next msu, adding remote ip %s","");
    	if(del_route_update->peer_ipv4 == 0){
    		log_error("peer ip is 0 for remote locality! %s","");
    		goto err;
    	}
    	del_entry.ipv4= del_route_update->peer_ipv4;
    }
    else{
    	log_error("ERROR: Unknown locality %u",del_entry.locality);
    	goto err;
    }

    rt_table = del_routing_table_entry(rt_table, del_route_update->peer_msu_type, &del_entry);
    log_debug("Deleted routing table entry for peer msu: %d",del_entry.id);
    print_routing_table(rt_table);

	return rt_table;
err:
	log_error("Failed to do route_update %s","");
	return NULL;

}

void free_msu_control_update(struct msu_control_update *update_msg){

	if(update_msg->payload){
		free(update_msg->payload);
	}
	free(update_msg);
}

struct msu_routing_table *handle_routing_table_update(struct msu_routing_table *rt_table, struct msu_control_update *update_msg){
	struct msu_control_add_route *add_update;
	struct msu_control_del_route *del_update;
	struct msu_routing_table *tmp_rt;

	add_update = NULL;
	del_update = NULL;

	//add or delete routing table entry could be the update_type
	if(update_msg->update_type == MSU_ROUTE_ADD){
		add_update = (struct msu_control_add_route*)update_msg->payload;
		tmp_rt = do_add_route_update(rt_table, add_update);
		if(tmp_rt != NULL){
			// this of the case when first entry is added, but works with all
			rt_table = tmp_rt;
		}
	}
	else if(update_msg->update_type == MSU_ROUTE_DEL){
		del_update = (struct msu_control_del_route*)update_msg->payload;
		tmp_rt = do_del_route_update(rt_table, del_update);
		if (tmp_rt != NULL) {
			// this of the case when last entry is removed, but works with all
			rt_table = tmp_rt;
		}
	}
	return rt_table;
}
