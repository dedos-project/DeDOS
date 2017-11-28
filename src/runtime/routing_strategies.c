/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
/**
 * @file routing_strategies.c
 * Defines strategies that MSUs can use for routing to endpoints
 */
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
    int rtn = get_route_endpoint(table, msg->hdr.key.id, output);
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
    int rtn = get_shortest_queue_endpoint(table, msg->hdr.key.id, output);
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
int route_to_origin_runtime(struct msu_type *type, struct local_msu *sender, struct msu_msg *msg,
                            struct msu_endpoint *output) {

    uint32_t runtime_id = msg->hdr.provinance.origin.runtime_id;

    struct routing_table *table = get_type_from_route_set(&sender->routes, type->id);
    if (table == NULL) {
         log_error("No routes available from msu %d to type %d",
                  sender->id, type->id);
        return -1;
    }

    int n_endpoints = get_n_endpoints(table);
    struct msu_endpoint eps[n_endpoints];

    int n_rt_endpoints = get_endpoints_by_runtime_id(table, runtime_id, eps, n_endpoints);
    if (n_rt_endpoints <= 0) {
        log_error("Could not get endpoint with runtime id %d", runtime_id);
        return -1;
    }

    log(LOG_ROUTING_DECISIONS, "Sending to one of %d endpoints with correct runtime",
        n_rt_endpoints);
    *output = eps[((unsigned int)msg->hdr.key.id) % n_rt_endpoints];
    log(LOG_ROUTING_DECISIONS, "Chose endpoint %d (idx: %d)",
        output->id, ((unsigned int)msg->hdr.key.id) % n_rt_endpoints);
    return 0;
}

