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
#include "dfg.h"
#include "controller_dfg.h"
#include "scheduling.h"
#include "api.h"
#include "logging.h"
#include "runtime_messages.h"
#include "controller_stats.h"
#include "msu_ids.h"
#include "haproxy.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
static int UNUSED n_downstream_msus(struct dfg_msu * msu) {

    int n = 0;
    struct dfg_route **routes = msu->scheduling.routes;
    for (int r_i = 0; r_i < msu->scheduling.n_routes; ++r_i) {
        struct dfg_route *r = routes[r_i];
        n+= r->n_endpoints;
    }
    return n;
}

#define QLEN_ROUTING

#ifdef QLEN_ROUTING

static double get_q_len(struct dfg_msu *msu) {
    struct timed_rrdb *q_len = get_msu_stat(QUEUE_LEN, msu->id);
    if (q_len == NULL || q_len->used ==  0) {
        return 0;
    }
    return q_len->data[(q_len->write_index + RRDB_ENTRIES - 1) % RRDB_ENTRIES];
}

int downstream_q_len(struct dfg_msu *msu) {
    double qlen = get_q_len(msu);
    struct dfg_route **routes = msu->scheduling.routes;
    for (int r_i = 0; r_i < msu->scheduling.n_routes; ++r_i) {
        struct dfg_route *r = routes[r_i];
        for (int i=0; i<r->n_endpoints; i++) {
            qlen += get_q_len(r->endpoints[i]->msu);
        }
    }
    return qlen;
}
#endif

#ifdef QLEN_ROUTING
static int fix_route_ranges(struct dfg_route *route) {

    if (route->msu_type->id == WEBSERVER_HTTP_MSU_TYPE_ID) {
        return 0;
    }

    if (route->n_endpoints <= 1) {
        return 0;
    }

    double total_q_len = 0;
    double q_lens[route->n_endpoints];
    for (int i=0; i < route->n_endpoints; i++) {
        q_lens[i] = downstream_q_len(route->endpoints[i]->msu);
        if (q_lens[i] < .001) {
            q_lens[i] = .001;
        }
        total_q_len += q_lens[i];
    }
    int last = 0;
    int keys[route->n_endpoints];
    int old_keys[route->n_endpoints];
    int ids[route->n_endpoints];
    for (int i=0; i < route->n_endpoints; i++) {
        double pct = (1.0 - q_lens[i] / total_q_len) * 10000;
        if (pct < 1) {
            pct = 1;
        }
        keys[i] = pct + last;
        last = keys[i];
        old_keys[i] = route->endpoints[i]->key;
        ids[i] = route->endpoints[i]->msu->id;
    }

    for (int i=0; i < route->n_endpoints; i++) {
        if (old_keys[i] != keys[i]) {
            int rtn = mod_endpoint(ids[i], keys[i], route->id);
             if (rtn < 0) {
                log_error("Error modifying endpoint");
                return -1;
            } else {
                log(LOG_ROUTING_CHANGES,
                          "Modified endpoint %d (idx: %d) in route %d to have key %d (old: %d)",
                          ids[i], i, route->id, keys[i], old_keys[i]);
            }
        }
    }
    return 0;
}

#else

static int fix_route_ranges(struct dfg_route *route) {

    if (route->n_endpoints <= 1) {
        return 0;
    }

    // Calculate what the new keys will be based on the number of downstream MSUs
    int old_ids[route->n_endpoints];
    int old_keys[route->n_endpoints];
    int new_keys[route->n_endpoints];
    int last = 0;
    for (int i=0; i<route->n_endpoints; i++) {

        old_keys[i] = route->endpoints[i]->key;
        old_ids[i] = route->endpoints[i]->msu->id;

        double key = n_downstream_msus(route->endpoints[i]->msu);
        new_keys[i] = last + (key > 0 ? key : 1);
        last = new_keys[i];
    }

    int old_diff = old_keys[0];
    int new_diff =  new_keys[0];

    // If the differences between the keys is the same across all endpoints, no need to change
    int change_in_diffs = 0;
    for (int i=1; i<route->n_endpoints; i++) {
        if (old_keys[i] - old_keys[i-1] != old_diff ||
                new_keys[i] - new_keys[i-1] != new_diff) {
            change_in_diffs = 1;
            break;
        }
    }
    if (!change_in_diffs) {
        return 0;
    }


    for (int i=0; i<route->n_endpoints; i++) {
        if (old_keys[i] != new_keys[i]) {
            int rtn = mod_endpoint(old_ids[i], new_keys[i], route->id);
            if (rtn < 0) {
                log_error("Error modifying endpoint");
                return -1;
            } else {
                log(LOG_ROUTING_CHANGES,
                          "Modified endpoint %d (idx: %d) in route %d to have key %d (old: %d)",
                          old_ids[i], i, route->id, new_keys[i], old_keys[i]);
            }
        }
    }

    return 0;
}
#endif

int fix_all_route_ranges(struct dedos_dfg *dfg) {
    for (int i=0; i<dfg->n_runtimes; i++) {
        struct dfg_runtime *rt = dfg->runtimes[i];
        for (int j=0; j<rt->n_routes; j++) {
            int rtn = fix_route_ranges(rt->routes[j]);
            if (rtn < 0) {
                log_error("Error fixing route ranges");
                return -1;
            }
        }
    }
    set_haproxy_weights(0, 0);
    return 0;
}

/**
 * Based on the meta routing, sort msus in a list in ascending order (from leaf to root)
 * @param struct dfg_msu *msus: decayed pointer to a list of MSU pointers
 * @return -1/0: failure/success
 */
int msu_hierarchical_sort(struct dfg_msu **msus) {

    int n_msus;
    for (n_msus=0; msus[n_msus] != NULL; n_msus++);

    if (n_msus == 1) {
        return 0;
    }

    int i, j;
    //First find any "exit" vertex and swap them with the first entry
    //FIXME: atm assumes there is only 1 new exit node
    for (i = 0; i < n_msus; ++i) {
        struct dfg_msu *msu = msus[i];
        if (msu->vertex_type & EXIT_VERTEX_TYPE) {
            for (j = 0; j != i && j < n_msus; ++j) {
                if (!(msu->vertex_type & EXIT_VERTEX_TYPE)) {
                    struct dfg_msu *tmp = msus[j];
                    msus[j] = msus[i];
                    msus[i] = tmp;
                }
            }
        }
    }

    //Awful linear search to sort
    for (i = 0; i < n_msus; ++i) {
        int up;
        for (j = i+1; j < n_msus; ++j) {
            for (up = 0; up < msus[i]->type->meta_routing.n_dst_types; ++up) {
                struct dfg_msu_type *upt = msus[i]->type->meta_routing.dst_types[up];
                if (msus[j]->type == msus[i]->type->meta_routing.dst_types[up] && j > i) {
                    struct dfg_msu *tmp = msus[j];
                    msus[j] = msus[i];
                    msus[i] = tmp;
                    i = 0;
                    j = 0;
                    break;
                }
                for (int upup = 0; upup < upt->meta_routing.n_dst_types; ++upup) {
                    if (msus[j]->type == upt->meta_routing.dst_types[upup] && j > i) {
                        struct dfg_msu *tmp = msus[j];
                        msus[j] = msus[i];
                        msus[i] = tmp;
                        i = 0;
                        j = 0;
                        break;
                    }
                }
            }
        }
    }

    return 0;
}

/**
 * Find an ID and clean up data structures for an MSU
 * @param dfg_msu msu: target MSU
 * @return none
 */
//FIXME: add error handling
void prepare_clone(struct dfg_msu  *msu) {
    msu->id = generate_msu_id();
    msu->scheduling.n_routes = 0;
}

/**
 * Find a suitable thread for an MSU
 * @param dfg_runtime *runtime: pointer to runtime endpoint
 * @param int colocation_group: colocation group ID to filter
 * @param int msu_type: MSU type ID to filter
 * @return: pointer to dfg_thread or NULL
 */
struct dfg_thread *find_unused_thread_except(struct dfg_runtime *runtime,
                                             struct dfg_msu_type *type,
                                             int is_pinned,
                                             int *bad_threads,
                                             int n_bad_threads) {
    for (int i=0; i<runtime->n_pinned_threads + runtime->n_unpinned_threads; i++) {
        struct dfg_thread *thread = runtime->threads[i];
        bool is_bad = false;
        for (int j=0; j < n_bad_threads; j++) {
            if (bad_threads[j] == thread->id) {
                is_bad = true;
                break;
            }
        }
        if (is_bad) {
            continue;
        }
        if ( (!is_pinned && thread->mode == PINNED_THREAD) || (is_pinned && thread->mode != PINNED_THREAD)) {
            continue;
        }

        if (type->colocation_group == 0 && thread->n_msus > 0) {
            continue;
        }

        if (thread->n_msus == 0) {
            log(LOG_THREAD_DECISIONS, "Placing type %d on thread %d",
                       type->id, thread->id);
            return thread;
        }

        int cannot_place = 0;
        for (int j=0; j<thread->n_msus; ++j) {
            if (thread->msus[j]->type->colocation_group != type->colocation_group ||
                    thread->msus[j]->type == type) {
                cannot_place = 1;
                break;
            }
        }
        if (cannot_place) {
            continue;
        }

        log(LOG_THREAD_DECISIONS, "Placing type %d on thread %d",
                   type->id, thread->id);
        return thread;
    }
    return NULL;
}

struct dfg_thread *find_unused_thread(struct dfg_runtime *runtime, 
                                      struct dfg_msu_type *type,
                                      int is_pinned) {
    return  find_unused_thread_except(runtime, type, is_pinned, NULL, 0);
}

/**
 * Find a core on which to spawn a new thread for an MSU
 * @param dfg_runtime: the target runtime
 * @param struct dfg_msu_msu: the target MSU
 * @return failure/success: -1/0
 */
int place_on_runtime(struct dfg_runtime *rt, struct dfg_msu *msu) {
    int ret = 0;

    struct dfg_thread *free_thread = find_unused_thread(rt, msu->type,
                                                        msu->blocking_mode == NONBLOCK_MSU);
    //struct dfg_thread *free_thread = find_unused_pinned_thread(rt, msu);
    if (free_thread == NULL) {
        log(LOG_SCHEDULING, "There are no free worker threads on runtime %d", rt->id);
        return -1;
    }

    free_thread->msus[free_thread->n_msus] = msu;
    free_thread->n_msus++;
    //update local view
    msu->scheduling.thread = free_thread;
    msu->scheduling.runtime = rt;

    register_msu_stats(msu->id, msu->type->id, msu->scheduling.thread->id, msu->scheduling.runtime->id);
    ret = send_create_msu_msg(msu);
    if (ret == -1) {
        log_error("Could not send addmsu command to runtime %d", msu->scheduling.runtime->id);
        return ret;
    }

    //TODO: update rt->current_alloc

    return ret;
}

struct dfg_dependency *get_dependency(struct dfg_msu_type *type, struct dfg_msu_type *dep_type) {
    for (int i=0; i<type->n_dependencies; i++) {
        if (type->dependencies[i]->type == dep_type) {
            return type->dependencies[i];
        }
    }
    return NULL;
}

int wire_msu(struct dfg_msu *msu) {
    struct dfg_runtime *rt = msu->scheduling.runtime;
    int ret;

    //update routes
    int i, j;
    struct dfg_msu_type *type = msu->type;
    for (i = 0; i < type->meta_routing.n_dst_types; ++i) {

        struct dfg_msu_type *dst_type = type->meta_routing.dst_types[i];
        if (dst_type->n_instances < 1) {
            continue;
        }

        struct dfg_dependency *dependency = get_dependency(type, dst_type);
        int need_remote_dep = 0, need_local_dep = 0;

        if (dependency != NULL) {
            if (dependency->locality == MSU_IS_LOCAL ) {
                need_local_dep = 1;
            } else if (dependency->locality == MSU_IS_REMOTE ) {
                need_remote_dep = 1;
            }
        }

        for (j = 0; j < dst_type->n_instances; ++j) {
            struct dfg_msu *dst = dst_type->instances[j];

            if ((need_local_dep && dst->scheduling.runtime != msu->scheduling.runtime)
                ||
                (need_remote_dep && dst->scheduling.runtime == msu->scheduling.runtime)) {
                continue;
            } else {
                //Does the new MSU's runtime has a route toward destination MSU's type?
                struct dfg_route *route = get_dfg_rt_route_by_type(msu->scheduling.runtime,
                                                                   dst->type);
                if (route == NULL) {
                    log(LOG_SCHEDULING, "Route of type %d doesn't exist on rt %d. Creating",
                        dst->type->id, msu->scheduling.runtime->id);
                    int route_id = generate_route_id();

                    route = create_dfg_route(route_id, dst->type, rt->id);
                    if (route == NULL) {
                        log_error("Could not add new route on runtime %d toward type %d",
                              rt->id, dst->type->id);
                        return -1;
                    }
                    if (send_create_route_msg(route) != 0) {
                        log_error("Could not send create route message");
                        return -1;
                    }
                }

                //Is the destination MSU already an endpoint of that route?
                if (!get_dfg_route_endpoint(route, dst->id)) {
                    uint32_t key = generate_endpoint_key(route);
                    ret = add_endpoint(dst->id, key, route->id);
                    if (ret != 0) {
                        log_error("Could not add endpoint %d to route %d",
                                  dst->id, route->id);
                        return -1;
                    }
                }
                //Is the route already attached to the new MSU?
                if (!msu_has_route(msu, route)) {
                    ret = add_route_to_msu(msu->id, route->id);
                    if (ret != 0) {
                        log_error("Could not add route %d to msu %d", route->id, msu->id);
                        return -1;
                    }
                }

            }
        }
    }

    for (i = 0; i < type->meta_routing.n_src_types; ++i) {

        struct dfg_msu_type *src_type = type->meta_routing.src_types[i];
        if (src_type->n_instances < 1) {
            continue;
        }

        struct dfg_dependency *dependency = get_dependency(type, src_type);
        int need_remote_dep = 0, need_local_dep = 0;

        if (dependency != NULL) {
            if (dependency->locality == MSU_IS_LOCAL) {
                need_local_dep = 1;
            } else if (dependency->locality == MSU_IS_REMOTE) {
                need_remote_dep = 1;
            }
        }

        for (j = 0; j < src_type->n_instances; ++j) {
            struct dfg_msu *source = src_type->instances[j];

            if ((need_local_dep && source->scheduling.runtime != msu->scheduling.runtime)
                ||
                (need_remote_dep && source->scheduling.runtime == msu->scheduling.runtime)) {
                continue;
            } else {
                struct dfg_runtime *src_rt = source->scheduling.runtime;

                //Does the source's runtime has a route toward new MSU's type?
                struct dfg_route *route = get_dfg_rt_route_by_type(src_rt, msu->type);
                if (route == NULL) {
                    log(LOG_SCHEDULING, "Route of type %d doesn't exist from rt %d",
                        msu->type->id, src_rt->id);
                    int route_id = generate_route_id();
                    int ret = create_route(route_id, msu->type->id, src_rt->id);
                    if (ret != 0) {
                        log_error("Could not add new route on runtime %d toward type %d",
                                  src_rt->id, msu->type->id);
                        return -1;
                    }
                    route = get_dfg_rt_route_by_type(src_rt, msu->type);
                }

                struct dfg_route *msu_route = get_dfg_msu_route_by_type(source, msu->type);
                //Is the route attached to that source msu?
                if (msu_route == NULL) {
                    log(LOG_SCHEDULING, "Route %d doesn't exist from source %d",
                        route->id, source->id);
                    ret = add_route_to_msu(source->id, route->id);
                    if (ret != 0) {
                        log_error("Could not attach route %d to msu %d",
                                  source->id, route->id);
                        return -1;
                    }
                } else {
                    route = msu_route;
                }

                //Is the new MSU already an endpoint of that route?
                if (!get_dfg_route_endpoint(route, msu->id)) {
                    uint32_t key = generate_endpoint_key(route);
                    ret = add_endpoint(msu->id, key, route->id);
                    if (ret != 0) {
                        log_error("Could not add endpoint %d to route %d",
                                  msu->id, route->id);
                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}

static int remove_routes_to_msu(struct dfg_msu *msu) {
    struct dedos_dfg *dfg = get_dfg();
    for (int i=0; i<dfg->n_runtimes; i++) {
        struct dfg_runtime *rt = dfg->runtimes[i];
        for (int j=0; j<rt->n_routes; j++) {
            struct dfg_route *route = rt->routes[j];
            struct dfg_route_endpoint *ep = get_dfg_route_endpoint(route, msu->id);

            if (ep == NULL) {
                continue;
            }

            int rtn = del_endpoint(msu->id, route->id);
            if (rtn < 0) {
                log_error("Error deleting endpoint from route %d for removal of %d",
                          route->id, msu->id);
                return -1;
            }
        }
    }
    return 0;
}

static int get_dependencies(struct dfg_msu *msu, struct dfg_msu **output, int out_size) {
    output[0] = msu;
    int n_on_runtime = 0;
    struct dfg_runtime *rt = msu->scheduling.runtime;
    for (int i=0; i<msu->type->n_instances; i++) {
        if (msu->type->instances[i]->scheduling.runtime == rt) {
            n_on_runtime++;
            if (n_on_runtime > 1) {
                // More than one on this runtime, just return the passed-in MSU
                return 1;
            }
        }
    }

    int n_out = 1;
    for (int i=0; i<msu->type->n_dependencies; i++) {
        struct dfg_dependency *dep = msu->type->dependencies[i];
        // TODO: Check dep->locality
        struct dfg_msu_type *dep_type = dep->type;
        for (int j=0; j<dep_type->n_instances; j++) {
            struct dfg_msu *dep_msu = dep_type->instances[j];
            if (dep_msu->scheduling.runtime == rt) {
                if (n_out >= out_size - 1) {
                    log_error("Cannot get dependencies -- output too small");
                    return -1;
                }
                output[n_out] = dep_msu;
                n_out++;
            }
        }
    }
    output[n_out] = NULL;
    msu_hierarchical_sort(output);
    return n_out;
}


int unclone_msu(int msu_id) {

    struct dfg_msu *msu = get_dfg_msu(msu_id);

    if (msu == NULL) {
        log_error("Cannot unclone MSU %d. Does not exist!", msu_id);
        return -1;
    }

    if (msu->type->n_instances <= 1) {
        log(LOG_SCHEDULING, "Cannot remove last instance of msu type %s", msu->type->name);
        return -1;
    }


    struct dfg_msu *dependencies[MAX_MSU];
    int n_deps = get_dependencies(msu, dependencies, MAX_MSU);

    for (int i=n_deps-1; i>=0; i--) {
        if (dependencies[i]->type->id == WEBSERVER_READ_MSU_TYPE_ID) {
            set_haproxy_weights(dependencies[i]->scheduling.runtime->id, 1);
        }
        int rtn = remove_routes_to_msu(dependencies[i]);
        if (rtn < 0) {
            log_error("Error removing routes to msu %d", dependencies[i]->id);
            return -1;
        }
        int id = dependencies[i]->id;
        char *name = dependencies[i]->type->name;
        rtn = remove_msu(id);
        if (rtn < 0) {
            log_error("Error removing MSU %d", id);
            return -1;
        }
        log(LOG_CLONING, "Removed msu %d (type: %s)", id, name);

        //TODO: This should really wait for confirmation of deletion of MSU
        if (i > 0)
            usleep(2000e3); // 2 seconds
    }

    return 0;
}





/**
 * Clone a msu of given ID
 * @param msu_id: id of the MSU to clone
 * @return 0/-1 success/failure
 */
struct dfg_msu *clone_msu(int msu_id) {
    int ret;

    struct dfg_msu *clone = calloc(1, sizeof(struct dfg_msu));
    if (clone == NULL) {
        debug("Could not allocate memory for clone msu");
        return NULL;
    }

    struct dfg_msu *msu = get_dfg_msu(msu_id);

    if (msu->type->cloneable == 0) {
        debug("Cannot clone msu %d", clone->msu_id);
        unlock_dfg();
        return NULL;
    }

    memcpy(clone, msu, sizeof(struct dfg_msu));

    prepare_clone(clone);
    clone->type = msu->type;


    lock_dfg();
    struct dedos_dfg *dfg = get_dfg();
    struct dfg_msu *msus[MAX_MSU];
    bzero(msus, MAX_MSU * sizeof(struct dfg_msu *));

    int i;
    for (i = 0; i < dfg->n_runtimes; ++i) {
        if (msu->type->cloneable < dfg->runtimes[i]->id) {
            log_warn("Could not clone msu %d on runtime %d",
                     clone->id, dfg->runtimes[i]->id);
            continue;
        }
        ret = schedule_msu(clone, dfg->runtimes[i], msus);
        if (ret == 0) {
            break;
        } else {
            debug("Could not schedule msu %d on runtime %d",
                  clone->msu_id, dfg->runtimes[i]->id);
        }
    }

    ret = msu_hierarchical_sort(msus);
    if (ret == -1) {
        debug("Could not sort MSUs");
    }

    usleep(50000);

    int n = 0;
    while (msus[n] != NULL) {
        ret = wire_msu(msus[n]);
        if (ret == -1) {
            debug("Could not update routes for msu %d", msus[n]->msu_id);
            unlock_dfg();
            return NULL;
        }

        n++;
    }

    if (clone->scheduling.runtime == NULL) {
        debug("Unable to clone msu %d of type %d", msu->msu_id, msu->msu_type);
        free(clone);
        unlock_dfg();
        return NULL;
    } else {
        log_info("Cloned msu %d of type %d into msu %d on runtime %d",
              msu->id, msu->type->id, clone->id, clone->scheduling.runtime->id);

        int rtn = fix_all_route_ranges(dfg);
        if (rtn < 0) {
            log_error("Unable to properly modify route ranges");
            unlock_dfg();
            return NULL;
        }
        log_debug("properly changed route ranges");
        unlock_dfg();
        log(LOG_CLONING, "Cloned MSU %d", clone->id);
        return clone;
    }
}

bool could_clone_on_runtime(struct dfg_msu_type *type, struct dfg_runtime *rt, 
                            int used_threads[MAX_THREADS], int *n_used_threads,
                            int checked_types[MAX_MSU_TYPES], int *n_checked_types) {
    int is_blocking = type->instances[0]->blocking_mode == NONBLOCK_MSU;
    struct dfg_thread *free_thread = find_unused_thread_except(rt, 
                                                               type,
                                                               is_blocking,
                                                               used_threads,
                                                               *n_used_threads);
    if (free_thread == NULL) {
        return false;
    }

    checked_types[*n_checked_types] = type->id;
    (*n_checked_types)++;
    used_threads[*n_used_threads] = free_thread->id;
    (*n_used_threads)++;

    for (int i=0; i < type->n_dependencies; i++) {
        struct dfg_dependency *dep = type->dependencies[i];
        if (dep->locality != MSU_IS_LOCAL) {
            continue;
        }
        struct dfg_msu_type *dep_type = dep->type;
        bool already_checked = false;
        for (int j=0; j < *n_checked_types; j++) {
            if (dep_type->id == checked_types[j]) {
                already_checked = true;
                break;
            }
        }
        if (already_checked) {
            continue;
        }

        bool could_clone = could_clone_on_runtime(dep_type, rt, 
                                                  used_threads, n_used_threads,
                                                  checked_types, n_checked_types);
        if (could_clone == false) {
            return false;
        }
    }
    return true;
}

    

bool could_clone_type(struct dfg_msu_type *type) {
    struct dedos_dfg *dfg = get_dfg();

    for (int i=0; i < dfg->n_runtimes; i++) {
        int used_threads[MAX_THREADS];
        int checked_types[MAX_MSU_TYPES];
        int n_used_threads = 0;
        int n_checked_types = 0;
        if (could_clone_on_runtime(type, dfg->runtimes[i], used_threads, &n_used_threads, 
                                   checked_types, &n_checked_types)) {
            return true;
        }
    }
    return false;
}

/**
 * Tries to place an MSU on a given runtime. Will wire the MSU with all upstream and downstream
 * MSU, enforcing locality constraints. Also, will spawn any missing dependency.
 * @param struct dfg_msu msu: the target MSU
 * @param struct dfg_runtime: the target runtime
 * @return 0/-1 success/failure
 */
int schedule_msu(struct dfg_msu *msu, struct dfg_runtime *rt, struct dfg_msu **new_msus) {
    int ret;
    // First of all, find a thread on the runtime
    ret = place_on_runtime(rt, msu);
    if (ret == -1) {
        log(SCHEDULING, "Could not spawn a new worker thread on runtime %d", rt->id);
        return -1;
    }

    struct dedos_dfg *dfg = get_dfg();


    //Add the new MSU to the list of new additions
    memcpy(new_msus, &msu, sizeof(struct dfg_msu *));

    //Handle dependencies before wiring
    int l;
    int skipped = 0;
    dfg->msus[dfg->n_msus] = msu;
    dfg->n_msus++;
    msu->type->instances[msu->type->n_instances] = msu;
    msu->type->n_instances++;
    for (l = 0; l < msu->type->n_dependencies; ++l) {
        struct dfg_msu_type *dep_type = msu->type->dependencies[l]->type;
        if (msu->type->dependencies[l]->locality == MSU_IS_LOCAL) {
            struct dfg_msu *existing = msu_type_on_runtime(msu->scheduling.runtime, dep_type);
            if (existing== NULL) {
                if (dep_type->n_instances == 0) {
                    log_error("No instances to spawn from!");
                    return -1;
                }
                struct dfg_msu *template = dep_type->instances[0];
                //spawn missing local dependency
                struct dfg_msu *dep = copy_dfg_msu(template);

                if (dep == NULL) {
                    log_error("Could not allocate memory for missing dependency");
                    return -1;
                }


                prepare_clone(dep);
                dep->type = msu->type->dependencies[l]->type;

                ret = schedule_msu(dep, rt, new_msus + l + 1 - skipped);
                if (ret == -1) {
                    // FIXME: This doesn't properly remove the original MSUs
                    log_info("Could not schedule dependency %d on runtime %d",
                          dep->id, rt->id);
                    dfg->n_msus--;
                    free(dep);
                    return -1;
                } else {
                    debug("Scheduled dependency %d on runtime %d (l=%d)" , dep->msu_id, rt->id, l);
                }
            } else {
                log_debug("Already has dependency %d", dep_type->id);
                skipped++;
            }
        } else {
            log_warn("non-local dependency %d", dep_type->id);
            skipped++;
            //TODO: check for remote dependency
        }
    }

    log_debug("Processed %d dependencies", l);
    return 0;
}
