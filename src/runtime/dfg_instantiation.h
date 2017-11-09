/**
 * @file dfg_instantiation.h
 *
 * Instantiation of a dfg on a runtime
 */
#ifndef DFG_INSTANTIATION_H_
#define DFG_INSTANTIATION_H_

/**
 * Runs the runtime initilization function for the given MSU types
 * @param msu_types list of pointers to MSU types
 * @param n_msu_types Size of `msu_types`
 * @return 0 on success, -1 on error
 */
int init_dfg_msu_types(struct dfg_msu_type **msu_types, int n_msu_types);

/**
 * Instantiates the MSUs, routes, and threads on the specified runtime
 */
int instantiate_dfg_runtime(struct dfg_runtime *rt);

#endif
