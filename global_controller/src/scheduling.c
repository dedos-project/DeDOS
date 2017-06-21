#include <stdlib.h>
#include <string.h>
#include "dfg.h"
#include "scheduling.h"

/**
 * Find an ID and clean up data structures for an MSU
 * @param dfg_vertex msu: target MSU
 * @return none
 */
void prepare_clone(struct dfg_vertex *msu) {
    msu->msu_id = generate_msu_id();

    memset(&msu->scheduling, '\0', sizeof(struct msu_scheduling));

    msu->statistics.queue_items_processed.write_index = 0;
    memset(msu->statistics.queue_items_processed.data, 0, RRDB_ENTRIES);
    memset(msu->statistics.queue_items_processed.time, 0, RRDB_ENTRIES);
    msu->statistics.queue_length.write_index = 0;
    memset(msu->statistics.queue_length.data, 0, RRDB_ENTRIES);
    memset(msu->statistics.queue_length.time, 0, RRDB_ENTRIES);
    msu->statistics.memory_allocated.write_index = 0;
    memset(msu->statistics.memory_allocated.data, 0, RRDB_ENTRIES);
    memset(msu->statistics.memory_allocated.time, 0, RRDB_ENTRIES);
}

/**
 * Find a core on which to spawn a new thread for an MSU
 * @param dfg_runtime_endpoint: the target runtime
 * @param struct dfg_vertex_msu: the target MSU
 * @return failure/success: -1/0
 */
int place_on_runtime(struct dfg_runtime_endpoint *rt, struct dfg_vertex *msu) {
    if (rt->num_pinned_threads > rt->num_cores) {
        debug("There are no more free cores to pin a new worker thread on runtime %d", rt->id);
        return -1;
    } else {
        int ret = 0;

        sleep(5); // To leave time to runtime to digest the command...
        //commit change to runtime
        ret = create_worker_thread(rt->sock);
        if (ret == -1) {
            debug("Could not create new worker thread on runtime %d", rt->id);
            return ret;
        }
        sleep(5); // To leave time to runtime to digest the command...

        //update local view
        msu->scheduling.thread_id = rt->num_pinned_threads;
        msu->scheduling.runtime = rt;

        char *init_data = NULL;
        ret = send_addmsu_msg(msu, init_data);
        if (ret == -1) {
            debug("Could not send addmsu command to runtime %d", msu->scheduling.runtime->id);
            return ret;
        }

        //TODO: update rt->current_alloc

        return ret;
    }
}

/**
 * Clone a msu of given ID
 * @param msu_id: id of the MSU to clone
 * @return 0/-1 success/failure
 */
int clone_msu(int msu_id) {
    struct dfg_vertex *clone = malloc(sizeof(struct dfg_vertex));
    if (clone == NULL) {
        debug("Could not allocate memory for clone msu");
        return -1;
    }

    struct dfg_vertex *msu = get_msu_from_id(msu_id);
    memcpy(clone, msu, sizeof(*msu));

    prepare_clone(clone);

    struct dfg_config *dfg = get_dfg();
    int i;
    for (i = 0; i < dfg->runtimes_cnt; ++i) {
        int ret = schedule_msu(clone, dfg->runtimes[i]);
        if (ret == 0) {
            break;
        } else {
            debug("Could not schedule msu %d on runtime %d",
                  clone->msu_id, dfg->runtimes[i]->id);
        }
    }

    if (clone->scheduling.runtime == NULL) {
        debug("Unable to clone msu %d of type %d", msu->msu_id, msu->msu_type);
        free(clone);
        return 1;
    } else {
        debug("Cloned msu %d of type %d into msu %d on runtime %d",
              msu->msu_id, msu->msu_type, clone->msu_id, clone->scheduling.runtime->id);
        return 0;
    }
}

/**
 * Tries to place an MSU on a given runtime. Will wire the MSU with all upstream and downstream
 * MSU, enforcing locality constraints. Also, will spawn any missing dependency.
 * @param struct dfg_vertex msu: the target MSU
 * @param struct dfg_runtime_endpoint: the target runtime
 * @return 0/-1 success/failure
 */
int schedule_msu(struct dfg_vertex *msu, struct dfg_runtime_endpoint *rt) {
    int ret;
    // First of all, find a thread on the runtime
    ret = place_on_runtime(rt, msu);
    if (ret == -1) {
        debug("Could not spawn a new worker thread on runtime %d", rt->id);
        return -1;
    }

    struct dfg_config *dfg = get_dfg();
    dfg->vertices[dfg->vertex_cnt] = msu;
    dfg->vertex_cnt++;

    //update routes
    int i, j;
    for (i = 0; i < msu->meta_routing.num_dst_types; ++i) {
        struct msus_of_type *dst_types =
            get_msus_from_type(msu->meta_routing.dst_types[i]);

        if (dst_types->num_msus < 1) {
            continue;
        }

        struct dependent_type *dependency = get_dependent_type(msu, dst_types->type);
        int need_remote_dep = 0, need_local_dep = 0;

        if (dependency != NULL) {
            if (dependency->locality == 1 ) {
                need_local_dep = 1;
            } else if (dependency->locality == 0) {
                need_remote_dep = 1;
            }
        }

        for (j = 0; j < dst_types->num_msus; ++j) {
            sleep(5);
            struct dfg_vertex *dst = get_msu_from_id(dst_types->msu_ids[j]);

            if ((need_local_dep && dst->scheduling.runtime->id != msu->scheduling.runtime->id)
                ||
                (need_remote_dep && dst->scheduling.runtime->id == msu->scheduling.runtime->id)) {
                continue;
            } else {
                int runtime_index = -1;
                runtime_index = get_sock_endpoint_index(rt->sock);
                if (runtime_index == -1) {
                    debug("Couldn't find endpoint index for sock: %d", rt->sock);
                    return -1;
                }

                //Does the new MSU's runtime has a route toward destination MSU's type?
                //FIXME: we need to send the create route command on the runtime.
                struct dfg_route *route = get_route_from_type(rt, dst->msu_type);
                if (route == NULL) {
                    int route_id = generate_route_id(rt);
                    ret = dfg_add_route(rt, route_id, dst->msu_type);
                    if (ret != 0) {
                        debug("Could not add new route on runtime %d toward type %d",
                              rt->id, dst->msu_type);
                        return -1;
                    }
                    route = get_route_from_type(rt, dst->msu_type);
                }

                //Add the route to the new MSU
                ret = add_route_to_msu_vertex(runtime_index, msu->msu_id, route->route_id);
                if (ret != 0) {
                    debug("Could not attach route %d to msu %d",
                          dst->msu_id, route->route_id);
                    return -1;
                }

                ret = send_addroute_msg(route->route_id, msu->msu_id, rt->sock);
                if (ret != 0) {
                    debug("Could not send add route %d msg to runtime %d",
                          route->route_id, rt->id);
                    return -1;
                }

                //Is the destination MSU already an endpoint of that route?
                if (!route_has_endpoint(route, dst)) {
                    int max_range = increment_max_range(route);
                    ret = dfg_add_route_endpoint(runtime_index, route->route_id, dst->msu_id, max_range);
                    if (ret != 0) {
                        debug("Could not add endpoint %d to route %d",
                              dst->msu_id, route->route_id);
                        return -1;
                    }

                    ret = send_addendpoint_msg(dst->msu_id, runtime_index,
                                               route->route_id, max_range, rt->sock);
                    if (ret != 0) {
                        debug("Could not send add endpoint %d msg to runtime %d",
                              dst->msu_id, rt->id);
                        return -1;
                    }
                }
            }
        }
    }

    for (i = 0; i < msu->meta_routing.num_src_types; ++i) {
        struct msus_of_type *source_types =
            get_msus_from_type(msu->meta_routing.src_types[i]);

        if (source_types->num_msus < 1) {
            continue;
        }

        struct dependent_type *dependency = get_dependent_type(msu, source_types->type);
        int need_remote_dep = 0, need_local_dep = 0;

        if (dependency != NULL) {
            if (dependency->locality == 1 ) {
                need_local_dep = 1;
            } else if (dependency->locality == 0) {
                need_remote_dep = 1;
            }
        }

        for (j = 0; j < source_types->num_msus; ++j) {
            struct dfg_vertex *source = get_msu_from_id(source_types->msu_ids[j]);

            if ((need_local_dep && source->scheduling.runtime->id != msu->scheduling.runtime->id)
                ||
                (need_remote_dep && source->scheduling.runtime->id == msu->scheduling.runtime->id)) {
                continue;
            } else {
                struct dfg_runtime_endpoint *src_rt = source->scheduling.runtime;
                int runtime_index = -1;
                runtime_index = get_sock_endpoint_index(src_rt->sock);
                if (runtime_index == -1) {
                    debug("Couldn't find endpoint index for sock: %d", src_rt->sock);
                    return -1;
                }

                //Does the source's runtime has a route toward new MSU's type?
                //FIXME: we need to send the create route command on the runtime.
                struct dfg_route *route = get_route_from_type(src_rt, msu->msu_type);
                if (route == NULL) {
                    int route_id = generate_route_id(src_rt);
                    ret = dfg_add_route(rt, route_id, msu->msu_type);
                    if (ret != 0) {
                        debug("Could not add new route on runtime %d toward type %d",
                              src_rt->id, msu->msu_type);
                        return -1;
                    }
                    route = get_route_from_type(src_rt, msu->msu_type);
                }

                //Is the route attached to that source msu?
                if (!msu_has_route(source, route->route_id)) {
                    ret = add_route_to_msu_vertex(runtime_index, source->msu_id, route->route_id);
                    if (ret != 0) {
                        debug("Could not attach route %d to msu %d",
                              source->msu_id, route->route_id);
                        return -1;
                    }

                    ret = send_addroute_msg(route->route_id, source->msu_id, src_rt->sock);
                    if (ret != 0) {
                        debug("Could not send add route %d msg to runtime %d",
                              route->route_id, src_rt->id);
                        return -1;
                    }
                }

                //Add the new MSU as an endpoint for the route
                int max_range = increment_max_range(route);
                ret = dfg_add_route_endpoint(runtime_index, route->route_id, msu->msu_id, max_range);
                if (ret != 0) {
                    debug("Could not add endpoint %d to route %d",
                          msu->msu_id, route->route_id);
                    return -1;
                }

                ret = send_addendpoint_msg(msu->msu_id, runtime_index,
                                           route->route_id, max_range, src_rt->sock);
                if (ret != 0) {
                    debug("Could not send add endpoint %d msg to runtime %d",
                          msu->msu_id, src_rt->id);
                    return -1;
                }
            }
        }
    }

    //Now handle dependencies which were not directly connected vertices
    int l;
    for (l = 0; l < msu->num_dependencies; ++l) {
        int has_dep = 0;
        if (msu->dependencies[l]->locality == 1) {
            has_dep =
                lookup_type_on_runtime(msu->scheduling.runtime, msu->dependencies[l]->msu_type);
            if (!has_dep) {
                //spawn missing local dependency
                struct dfg_vertex *dep = malloc(sizeof(struct dfg_vertex));
                if (dep == NULL) {
                    debug("Could not allocate memory for missing dependency");
                    return -1;
                }

                prepare_clone(dep);
                dep->msu_type = msu->dependencies[l]->msu_type;
                clone_type_static_data(dep);

                sleep(1); // leave poor baby the time to digest
                ret = schedule_msu(dep, rt);
                if (ret == -1) {
                    debug("Could not schedule dependency %d on runtime %d",
                          dep->msu_id, rt->id);
                    free(dep);
                    return -1;
                }
            }
        } else {
            //TODO: check for remote dependency
        }
    }

    return 0;
}

/**
 * Find the least loaded suitable thread in a runtime to place an MSU
 */
struct runtime_thread *find_thread(struct dfg_vertex *msu, struct dfg_runtime_endpoint *runtime) {
    struct runtime_thread *least_loaded = NULL;
    int i;
    //for now we assume that all MSUs are to be scheduled on pinned threads
    for (i = 0; i < runtime->num_pinned_threads; ++i) {
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

/**
 * Lookup a cut for the presence of a give MSU
 * @param struct cut *c the cut
 * @param int msu_id the msu id
 * @return int 1/0 yes/no
 */
int is_in_cut(struct cut *c, int msu_id) {
    int i;
    for (i = 0; i < c->num_msu; i++) {
        if (c->msu_ids[i] == msu_id) {
            return 1;
        }
    }

    return 0;
}

/**
 * Compute in/out traffic requirement for a given msu
 * @param struct dfg_vertex *msu the desired msu
 * @param struct cut *c actual cut proposal
 * @param const char *direction egress/ingress
 * @return uint64_t bw requirement in the asked direction
 */
uint64_t compute_out_of_cut_bw(struct dfg_vertex *msu, struct cut *c, const char *direction) {
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
                    struct dfg_vertex *inc_msu = get_msu_from_id(t->msu_ids[m]);
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

/**
 * Pack as many MSU as possible on the same runtime in a first fit manner
 * @param struct to_schedule *ts information about the msu to be scheduled
 * @param struct dfg_config *dfg an instance of the DFG
 * @return int allocation successful/unsuccessful
 */
int greedy_policy(struct to_schedule *ts, struct dfg_config *dfg) {
    int num_to_allocate = ts->num_msu;
    int n;
    for (n = 0; n < dfg->runtimes_cnt; ++n) {
        struct dfg_runtime_endpoint *r = NULL;
        r = dfg->runtimes[n];

        /* Wipe out runtime's allocation */
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
                struct dfg_vertex *msu = get_msu_from_id(msu_id);

                //Check memory constraint
                uint64_t new_cut_dram = msu->profiling.dram + c.dram;
                if (new_cut_dram > r->dram) {
                    debug("Not enough dram on runtime %d to host msu %d",
                          r->id, msu_id);
                    continue;
                }

                //Check that one of the runtime's thread can host the msu
                struct runtime_thread *thread = NULL;
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

/* For now assume one instance of each msu (i.e one edge from src to dst */
/* FIXME: edge_set are no longer a thing
int set_edges(struct to_schedule *ts, struct dfg_config *dfg) {
    int n;
    for (n = 0; n < ts->num_msu; ++n) {
        int msu_id = ts->msu_ids[n];
        struct dfg_vertex *msu = get_msu_from_id(msu_id);

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

int set_msu_deadlines(struct to_schedule *ts, struct dfg_config *dfg) {
    //for now divide evenly the application deadline among msus
    float msu_deadline = dfg->application_deadline / dfg->vertex_cnt;

    int m;
    for (m = 0; m < ts->num_msu; ++m) {
        int msu_id = ts->msu_ids[m];
        struct dfg_vertex *msu = get_msu_from_id(msu_id);
        msu->scheduling.deadline = msu_deadline;
    }

    return 0;
}

/* Determine the number of initial instance of each MSU, and assign them to computers */
int allocate(struct to_schedule *ts) {
    int ret;
    // create a new dfg structure
    struct dfg_config *tmp_dfg, *dfg;
    tmp_dfg = malloc(sizeof(struct dfg_config));
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
    /*
    ret = set_edges(ts, tmp_dfg);
    if (ret != 0) {
        debug("error while creating edges for msu to allocate");
        free(tmp_dfg);
        free(ts);

        return -1;
    }
    */

    ret = policy(ts, tmp_dfg);
    if (ret != 0) {
        debug("error while applying allocation policy");
        free(tmp_dfg);
        free(ts);

        return -1;
    }

    // if validated "commit" by replacing current DFG by new one
    pthread_mutex_lock(&dfg->dfg_mutex);

    memcpy(dfg, tmp_dfg, sizeof(struct dfg_config));
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
