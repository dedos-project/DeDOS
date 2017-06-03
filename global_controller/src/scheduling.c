#include <stdlib.h>
#include <string.h>
#include "dfg.h"
#include "scheduling.h"


/**
 * Legacy greedy next thread finder
 * @param *runtime_sock placeholder for the target runtime socket
 * @param *thread_id id placeholder for the new thread id
 *
 */
int find_placement(int *runtime_sock, int *thread_id) {

    struct dfg_config *dfg = get_dfg();
    *runtime_sock = -1;
    *thread_id = -1;

    /**
     * For now we always place the new msu on a new thread. Thus this thread must be created on
     * the target runtime
     */
    int m;
    for (m = 0; m < dfg->runtimes_cnt; m++) {
        if (dfg->runtimes[m]->num_threads < dfg->runtimes[m]->num_cores) {
            *runtime_sock = dfg->runtimes[m]->sock;
            *thread_id    = dfg->runtimes[m]->num_threads;
            return 0;
        }
    }

    if (*runtime_sock == -1 && *thread_id == -1) {
        debug("ERROR: %s", "could not find runtime or thread to place new clone");
        return -1;
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

    dump_json();

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
