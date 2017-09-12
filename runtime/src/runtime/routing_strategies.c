#include "routing_strategies.h"
#include "local_msu.h"
#include "logging.h"
#include "msu_message.h"

int default_routing(struct msu_type *type, struct local_msu *sender,
                    struct msu_msg *msg, struct msu_endpoint *output) {
    
    struct routing_table *table = get_type_from_route_set(&sender->routes, type->id);
    if (table == NULL) {
        log_error("No routes available from msu %d to type %d",
                  sender->id, type->id);
        return -1;
    }
    int rtn = get_route_endpoint(table, msg->hdr->key.id, output);
    if (rtn < 0) {
        log_error("Error getting endpoint from msu %d", sender->id);
        return -1;
    }
    return 0;
}

int shortest_queue_route(struct msu_type *type, struct local_msu *sender,
                         struct msu_msg *msg, struct msu_endpoint *output) {
    struct routing_table *table = get_type_from_route_set(&sender->routes, type->id);
    if (table == NULL) {
        log_error("No routes available from msu %d to type %d",
                  sender->id, type->id);
        return -1;
    }
    int rtn = get_shortest_queue_endpoint(table, msg->hdr->key.id, output);
    if (rtn < 0) {
        log_error("Error getting shortest queue endpoint from msu %d", sender->id);
        return -1;
    }
    return 0;
}

int route_to_id(struct msu_type *type, struct local_msu *sender, int msu_id, 
                struct msu_endpoint *output) {
    struct routing_table *table = get_type_from_route_set(&sender->routes, type->id);
    if (table == NULL) {
        log_error("No routes available from msu %d to type %d",
                  sender->id, type->id);
        return -1;
    }
    int rtn = get_endpoint_by_id(table, msu_id, output);
    if (rtn < 0) {
        log_error("Error getting endpoint with ID %d from msu %d", msu_id, sender->id);
        return -1;
    }
    return 0;
}
