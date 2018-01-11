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
#include "runtime_messages.h"
#include "dfg.h"
#include "logging.h"
#include "ctrl_runtime_messages.h"
#include "runtime_communication.h"

int send_create_msu_msg(struct dfg_msu *msu) {

    if (msu->scheduling.runtime == NULL || msu->scheduling.thread == NULL) {
        log_error("Cannot send message for unscheduled MSU");
        return -1;
    }

    struct ctrl_create_msu_msg msg = {
        .msu_id = msu->id,
        .type_id = msu->type->id,
        .init_data = msu->init_data
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_CREATE_MSU,
        .thread_id = msu->scheduling.thread->id,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_runtime(msu->scheduling.runtime->id, &hdr, &msg);

    if (rtn < 0) {
        log_error("Error sending create-msu msg to runtime %d",
                  msu->scheduling.runtime->id);
        return -1;
    }
    log(LOG_RUNTIME_MESSAGES, "Sent CREATE_MSU message");
    return 0;
}

int send_delete_msu_msg(struct dfg_msu *msu) {

    if (msu->scheduling.runtime == NULL || msu->scheduling.thread == NULL) {
        log_error("Cannot send message for unscheduled MSU");
        return -1;
    }

    struct ctrl_delete_msu_msg msg = {
        .msu_id = msu->id,
        .force = 1
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_DELETE_MSU,
        .thread_id = msu->scheduling.thread->id,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_runtime(msu->scheduling.runtime->id, &hdr, &msg);

    if (rtn < 0) {
        log_error("Error sending delete-msu-msg to runtime %d",
                  msu->scheduling.runtime->id);
        return -1;
    }
    log(LOG_RUNTIME_MESSAGES, "Sent DELETE_MSU message");
    return 0;
}

int send_create_route_msg(struct dfg_route *route) {
    struct ctrl_route_msg msg = {
        .type = CREATE_ROUTE,
        .route_id = route->id,
        .type_id = route->msu_type->id
    };
    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_MODIFY_ROUTE,
        .thread_id = MAIN_THREAD_ID,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_runtime(route->runtime->id, &hdr, &msg);
    if (rtn < 0) {
        log_error("Error sending add-route msg to runtime %d", route->runtime->id);
        return -1;
    }
    log(LOG_RUNTIME_MESSAGES, "Sent CREATE_ROUTE message");
    return 0;
}

int send_delete_route_msg(struct dfg_route *route) {
    struct ctrl_route_msg msg = {
        .type = DELETE_ROUTE,
        .route_id = route->id
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_MODIFY_ROUTE,
        .thread_id = MAIN_THREAD_ID,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_runtime(route->runtime->id, &hdr, &msg);
    if (rtn < 0) {
        log_error("Error sending del-route msg to runtime %d", route->runtime->id);
        return -1;
    }
    log(LOG_RUNTIME_MESSAGES, "Sent DELETE_ROUTE message");
    return 0;
}

int send_add_route_to_msu_msg(struct dfg_route *route, struct dfg_msu *msu) {
    struct ctrl_msu_route_msg msg = {
        .type = ADD_ROUTE,
        .route_id = route->id,
        .msu_id = msu->id
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_MSU_ROUTES,
        .thread_id = msu->scheduling.thread->id,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_runtime(msu->scheduling.runtime->id, &hdr, &msg);
    if (rtn < 0) {
        log_error("Error sending add-route-to-msu msg to runtime %d",
                   msu->scheduling.runtime->id);
        return -1;
    }
    log(LOG_RUNTIME_MESSAGES, "Sent ADD_ROUTE_TO_MSU message");
    return 0;
}

int send_add_endpoint_msg(struct dfg_route *route, struct dfg_route_endpoint *endpoint) {
    struct ctrl_route_msg msg = {
        .type = ADD_ENDPOINT,
        .route_id = route->id,
        .msu_id = endpoint->msu->id,
        .key = endpoint->key,
        .runtime_id = endpoint->msu->scheduling.runtime->id
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_MODIFY_ROUTE,
        .thread_id = MAIN_THREAD_ID,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_runtime(route->runtime->id, &hdr, &msg);
    if (rtn < 0) {
        log_error("Error sending add-endpoint-to-route msg to runtime %d",
                  route->runtime->id);
        return -1;
    }
    log(LOG_RUNTIME_MESSAGES, "Sent ADD_ENDPOINT message");
    return 0;
}

int send_del_endpoint_msg(struct dfg_route *route, struct dfg_route_endpoint *endpoint) {
    struct ctrl_route_msg msg = {
        .type = DEL_ENDPOINT,
        .route_id = route->id,
        .msu_id = endpoint->msu->id
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_MODIFY_ROUTE,
        .thread_id = MAIN_THREAD_ID,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_runtime(route->runtime->id, &hdr, &msg);
    if (rtn < 0) {
        log_error("Error sending del-endpoint msg to runtime %d", route->runtime->id);
        return -1;
    }
    log(LOG_RUNTIME_MESSAGES, "Sent ADD_ENDPOINT message");
    return 0;
}

int send_mod_endpoint_msg(struct dfg_route *route, struct dfg_route_endpoint *endpoint) {
    struct ctrl_route_msg msg = {
        .type = MOD_ENDPOINT,
        .route_id = route->id,
        .msu_id = endpoint->msu->id,
        .key = endpoint->key
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_MODIFY_ROUTE,
        .thread_id = MAIN_THREAD_ID,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_runtime(route->runtime->id, &hdr, &msg);
    if (rtn < 0) {
        log_error("Error sending mod-endpoint msg to runtime %d", route->runtime->id);
        return -1;
    }
    return 0;
}

int send_create_thread_msg(struct dfg_thread *thread, struct dfg_runtime *rt) {
    struct ctrl_create_thread_msg msg = {
        .thread_id = thread->id,
        .mode = thread->mode
    };

    struct ctrl_runtime_msg_hdr hdr = {
        .type = CTRL_CREATE_THREAD,
        .thread_id = MAIN_THREAD_ID,
        .payload_size = sizeof(msg)
    };

    int rtn = send_to_runtime(rt->id, &hdr, &msg);
    if (rtn < 0) {
        log_error("Error sending create-thread msg to runtime %d", rt->id);
        return -1;
    }
    log(LOG_RUNTIME_MESSAGES, "Sent CREATE_THREAD message");
    return 0;
}
