#include "dfg.h"
#include "controller_dfg.h"
#include "scheduling.h"
#include "api.h"
#include "logging.h"
#include "runtime_messages.h"
#include "msu_stats.h"

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

#ifdef QLEN_ROUTING

static double get_q_len(struct dfg_msu *msu) {
    struct timed_rrdb *q_len = get_stat(MSU_QUEUE_LEN, msu->id);
    return average_n(q_len, 5);
}

static int downstream_q_len(struct dfg_msu *msu) {
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

static int fix_route_ranges(struct dfg_route *route) {

    // Calculate what the new keys will be based on the number of downstream MSUs
    int old_ids[route->n_endpoints];
    int old_keys[route->n_endpoints];
    int new_keys[route->n_endpoints];
    int last = 0;
    for (int i=0; i<route->n_endpoints; i++) {

        old_keys[i] = route->endpoints[i]->key;
        old_ids[i] = route->endpoints[i]->msu->id;
#ifdef QLEN_ROUTING
        double key = downstream_q_len(route->endpoints[i]->msu);

        if (down_q_len < 1) {
            new_keys[i] = last + 100;
        } else {
            new_keys[i] = last + (int)(100 / down_q_len + 1);
        }
#else
        double key = n_downstream_msus(route->endpoints[i]->msu);
        new_keys[i] = last + key;
#endif
        last = new_keys[i];
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
        struct dfg_msu *msu = msus[i];
        int up;
        for (up = 0; up < msu->type->meta_routing.n_dst_types; ++up) {
            for (j = 0; j != i && j < n_msus; ++j) {
                if (msus[j]->type == msu->type->meta_routing.dst_types[up] && j > i) {
                    struct dfg_msu *tmp = msus[j];
                    msus[j] = msus[i];
                    msus[i] = tmp;
                }
            }
        }
    }

    for (i = 0; i < n_msus; ++i) {
        log_debug("new msu n°%d has ID %d and type", i, msus[i]->msu_id, msus[i]->msu_type);
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
struct dfg_thread *find_unused_pinned_thread(struct dfg_runtime *runtime,
                                             struct dfg_msu_type *type) {
    for (int i=0; i<runtime->n_pinned_threads + runtime->n_unpinned_threads; i++) {
        struct dfg_thread *thread = runtime->threads[i];
        if (thread->mode != PINNED_THREAD) {
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
            if (thread->msus[i]->type->colocation_group != type->colocation_group ||
                    thread->msus[i]->type == type) {
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


/**
 * Find a core on which to spawn a new thread for an MSU
 * @param dfg_runtime: the target runtime
 * @param struct dfg_msu_msu: the target MSU
 * @return failure/success: -1/0
 */
int place_on_runtime(struct dfg_runtime *rt, struct dfg_msu *msu) {
    int ret = 0;

    struct dfg_thread *free_thread = find_unused_pinned_thread(rt, msu->type);
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

    register_stat_item(msu->id);
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

                //Is the route attached to that source msu?
                if (!msu_has_route(source, route)) {
                    ret = add_route_to_msu(source->id, route->id);
                    if (ret != 0) {
                        log_error("Could not attach route %d to msu %d",
                                  source->id, route->id);
                        return -1;
                    }
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
        debug("Cloned msu %d of type %d into msu %d on runtime %d",
              msu->msu_id, msu->msu_type, clone->msu_id, clone->scheduling.runtime->id);

        int rtn = fix_all_route_ranges(dfg);
        if (rtn < 0) {
            log_error("Unable to properly modify route ranges");
            unlock_dfg();
            return NULL;
        }
        log_debug("properly changed route ranges");
        unlock_dfg();
        return clone;
    }
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
        log_error("Could not spawn a new worker thread on runtime %d", rt->id);
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

/*
 * Find the least loaded suitable thread in a runtime to place an MSU
 *
struct dfg_thread *find_thread(struct dfg_msu *msu, struct dfg_runtime *runtime) {
    struct dfg_thread *least_loaded = NULL;
    int i;
    //for now we assume that all MSUs are to be scheduled on pinned threads
    for (int i=0; i<runtime->n_pinned_threads + runtime->n_unpinned_threads; ++i) {
        if (runtime->threads[i]->mode != PINNED_THREAD) {
            continue;

        }
        float new_util = runtime->threads[i]->utilization +
                       (msu->profiling.wcet / msu->scheduling.deadline);

        if (new_util < 1) {
            if (least_loaded == NULL || new_util < least_loaded->utilization) {
                least_loaded = runtime->threads[i];
            }
        }


    }

    return least_loaded;
}
*/

/**
 * Lookup a cut for the presence of a give MSU
 * @param struct cut *c the cut
 * @param int msu_id the msu id
 * @return int 1/0 yes/no
 *
int is_in_cut(struct cut *c, int msu_id) {
    int i;
    for (i = 0; i < c->num_msu; i++) {
        if (c->msu_ids[i] == msu_id) {
            return 1;
        }
    }

    return 0;
}
*/

/**
 * Compute in/out traffic requirement for a given msu
 * @param struct dfg_msu *msu the desired msu
 * @param struct cut *c actual cut proposal
 * @param const char *direction egress/ingress
 * @return uint64_t bw requirement in the asked direction
 *
uint64_t compute_out_of_cut_bw(struct dfg_msu *msu, struct cut *c, const char *direction) {
    uint64_t out_of_cut_bw = 0;

    if (strncmp(direction, "ingress", strlen("ingress\0")) == 0) {
        //easiest way right now is to grab all the incoming MSU types,
        //and lookup for instances of those
        int i;
        for (i = 0; i < msu->meta_routing.num_src_types; ++i) {
            struct msus_of_type *t = NULL;
            t = get_msus_from_type(msu->meta_routing.src_types[i]);

            int m;
            for (m = 0; m < t->num_msus; ++m) {
                if (is_in_cut(c, t->msu_ids[m]) == 0) {
                    struct dfg_msu *inc_msu = get_msu_from_id(t->msu_ids[m]);
                    out_of_cut_bw += inc_msu->profiling.tx_node_remote;
                }
            }
        }
    } else if (strncmp(direction, "egress", strlen("egress\0")) == 0) {
        int i;
        //FIXME: update with new route objects
        //for (i = 0; i < msu->scheduling.num_routes; ++i) {
        //   if (is_in_cut(c, msu->scheduling.routes.edges[i]->to->msu_id) == 0) {
        //        out_of_cut_bw += msu->profiling.tx_node_remote;
        //    }
        //}
    }

    return out_of_cut_bw;
}
*/

/**
 * Pack as many MSU as possible on the same runtime in a first fit manner
 * @param struct to_schedule *ts information about the msu to be scheduled
 * @param struct dedos_dfg *dfg an instance of the DFG
 * @return int allocation successful/unsuccessful
 *
int greedy_policy(struct to_schedule *ts, struct dedos_dfg *dfg) {
    int num_to_allocate = ts->num_msu;
    int n;
    for (n = 0; n < dfg->runtimes_cnt; ++n) {
        struct dfg_runtime *r = NULL;
        r = dfg->runtimes[n];

        // Wipe out runtime's allocation 
        //WARNING: forbid partial allocation request for now
        if (r->current_alloc != NULL) {
            free(r->current_alloc);
        }

        //Temporary cut object on the stack
        struct cut c = {0, 0, 0, 0, 0, 0, NULL};

        int has_placed_msu;
        while (num_to_allocate > 0) {
            has_placed_msu = 0;

            int i;
            //as it is greedy, we should only iterate once through all msus to allocate
            for (i = 0; i < num_to_allocate; ++i) {
                int msu_id = ts->msu_ids[i];
                struct dfg_msu *msu = get_msu_from_id(msu_id);

                //Check memory constraint
                uint64_t new_cut_dram = msu->profiling.dram + c.dram;
                if (new_cut_dram > r->dram) {
                    debug("Not enough dram on runtime %d to host msu %d",
                          r->id, msu_id);
                    continue;
                }

                //Check that one of the runtime's thread can host the msu
                struct dfg_thread *thread = NULL;
                thread = find_thread(msu, r);
                if (thread == NULL) {
                    debug("Could not find a suitable thread on runtime %d to host msu %d",
                          r->id, msu_id);
                    continue;
                }
                //Check that the new ingress and egress bandwidth match the node's capacity
                uint64_t msu_ingress = compute_out_of_cut_bw(msu, &c, "ingress");
                uint64_t msu_egress = compute_out_of_cut_bw(msu, &c, "egress");
                uint64_t msu_bw = msu_ingress + msu_egress;

               if (msu_bw > 0 && (msu_bw + c.io_network_bw) > r->io_network_bw) {
                    debug("Not enough BW on runtime %d to host msu %d",
                          r->id, msu_id);
                    continue;
                }

                //All the constraints are satisfied, allocate the msu to the node
                msu->scheduling.runtime = r;
                msu->scheduling.thread_id = thread->id;
                msu->scheduling.thread = thread;

                //Update the cut
                c.dram = new_cut_dram;
                c.io_network_bw = msu_bw;
                c.egress_bw = msu_egress;
                c.ingress_bw = msu_ingress;
                c.msu_ids[c.num_msu] = msu->msu_id;
                c.num_msu++;

                struct cut *cut = NULL;
                cut = malloc(sizeof(struct cut));
                if (cut == NULL) {
                    debug("could not allocate memory to cut object");
                    return -1;
                }

                memcpy(cut, &c, sizeof(c));
                r->current_alloc = cut;

                num_to_allocate--;
                has_placed_msu = 1;
            }

            if (has_placed_msu == 0) {
                debug("%s", "can't place anymore MSU on this runtime");
                break;
            }
        }
    }

    if (num_to_allocate > 0) {
        return -1;
    }

    return 0;
}
*/

/* For now assume one instance of each msu (i.e one edge from src to dst */
/* FIXME: edge_set are no longer a thing
int set_edges(struct to_schedule *ts, struct dedos_dfg *dfg) {
    int n;
    for (n = 0; n < ts->num_msu; ++n) {
        int msu_id = ts->msu_ids[n];
        struct dfg_msu *msu = get_msu_from_id(msu_id);

        struct dfg_edge_set *edge_set = NULL;
        edge_set = malloc(sizeof(struct dfg_edge_set));
        if (edge_set == NULL) {
            debug("Could not allocate memory for dfg_edge_set");
            return -1;
        }
        int num_edges = 0;

        if (strncmp(msu->vertex_type, "exit", strlen("exit\0")) != 0) {
            struct msus_of_type *t = NULL;
            int s;
            for (s = 0; s < msu->meta_routing->num_dst_types; ++s) {
                t = get_msus_from_type(msu->meta_routing->dst_types[s]);

                int q;
                for (q = 0; q < t->num_msus; ++q) {
                    struct dfg_edge *e = NULL;
                    e = malloc(sizeof(struct dfg_edge));
                    if (e == NULL) {
                        debug("Could not allocate memory for dfg_edge");
                        return -1;
                    }

                    e->from = msu;
                    e->to = get_msu_from_id(t->msu_ids[q]);

                    edge_set->edges[edge_set->num_edges] = e;
                    edge_set->num_edges++;
                }
            }

            free(t);
        }

        msu->scheduling->routing = edge_set;
    }

    return 0;
}
*/
/*
int set_msu_deadlines(struct to_schedule *ts, struct dedos_dfg *dfg) {
    //for now divide evenly the application deadline among msus
    float msu_deadline = dfg->application_deadline / dfg->vertex_cnt;

    int m;
    for (m = 0; m < ts->num_msu; ++m) {
        int msu_id = ts->msu_ids[m];
        struct dfg_msu *msu = get_msu_from_id(msu_id);
        msu->scheduling.deadline = msu_deadline;
    }

    return 0;
}
*/

/* Determine the number of initial instance of each MSU, and assign them to computers 
int allocate(struct to_schedule *ts) {
    int ret;
    // create a new dfg structure
    struct dedos_dfg *tmp_dfg, *dfg;
    tmp_dfg = malloc(sizeof(struct dedos_dfg));
    dfg = get_dfg();

    //Are all declared runtime connected ?
    while (show_connected_peers() < dfg->runtimes_cnt) {
        debug("One of the declared runtime is not connected... call me back later");
        free(tmp_dfg);
        free(ts);

        return -1;
    }

    pthread_mutex_lock(&dfg->dfg_mutex);

    memcpy(tmp_dfg, dfg, sizeof(*dfg));

    pthread_mutex_unlock(&dfg->dfg_mutex);

    ret = set_msu_deadlines(ts, tmp_dfg);
    if (ret != 0) {
        debug("error while setting deadlines for msu to allocate");
        free(tmp_dfg);
        free(ts);

        return -1;
    }

    //TODO: set_num_instances() and then set_edges()
    //for now we assume one instance of each msu and define edges accordingly

    ret = set_edges(ts, tmp_dfg);
    if (ret != 0) {
        debug("error while creating edges for msu to allocate");
        free(tmp_dfg);
        free(ts);

        return -1;
    }
    *

    ret = policy(ts, tmp_dfg);
    if (ret != 0) {
        debug("error while applying allocation policy");
        free(tmp_dfg);
        free(ts);

        return -1;
    }

    // if validated "commit" by replacing current DFG by new one
    pthread_mutex_lock(&dfg->dfg_mutex);

    memcpy(dfg, tmp_dfg, sizeof(struct dedos_dfg));
    free(dfg);
    dfg = tmp_dfg;
    tmp_dfg = NULL;

    pthread_mutex_unlock(&dfg->dfg_mutex);

    free(ts);

    return 0;
}

int init_scheduler(const char *policy_name) {
    if (strcmp(policy_name, "greedy") == 0) {
        policy = greedy_policy;
    } else {
        policy = greedy_policy;
    }

    return 0;
}
*/
