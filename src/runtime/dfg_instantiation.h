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
