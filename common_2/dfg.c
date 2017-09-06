#include "dfg.h"
#include "logging.h"

#define SEARCH_FOR_ID(s, n, field, id_) \
    for (int i=0; i<(n); ++i) { \
        if (field[i]->id == id_) { \
            return field[i]; \
        } \
    } \
    return NULL;

struct dfg_runtime *get_dfg_runtime(struct dedos_dfg *dfg, int runtime_id) {
    SEARCH_FOR_ID(dfg, dfg->n_runtimes, dfg->runtimes, runtime_id)
}

struct dfg_msu_type *get_dfg_msu_type(struct dedos_dfg *dfg, int id){
    SEARCH_FOR_ID(dfg, dfg->n_msu_types, dfg->msu_types, id)
}

struct dfg_route *get_dfg_runtime_route(struct dfg_runtime *rt, int id) {
    SEARCH_FOR_ID(rt, rt->n_routes, rt->routes, id)
}

struct dfg_route *get_dfg_route(struct dedos_dfg *dfg, int id) {
    for (int i=0; i<dfg->n_runtimes; i++) {
        struct dfg_route *route = get_dfg_runtime_route(dfg->runtimes[i], id);
        if (route != NULL) {
            return route;
        }
    }
    return NULL;
}

struct dfg_msu *get_dfg_msu(struct dedos_dfg *dfg, int id){
    SEARCH_FOR_ID(dfg, dfg->n_msus, dfg->msus, id)
}

struct dfg_thread *get_dfg_thread(struct dfg_runtime *rt, int id){
    SEARCH_FOR_ID(rt, rt->n_pinned_threads + rt->n_unpinned_threads, rt->threads, id)
}
