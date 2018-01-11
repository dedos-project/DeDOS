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
 * @file runtime_dfg.h
 * Interactions with global dfg from individual runtime
 */
#ifndef RUNTIME_DFG_H_
#define RUNTIME_DFG_H_
#include <netinet/ip.h>
#include "dfg.h"

/**
 * Sets the local runtime to be equal to the provided rt.
 * Really should only be used from testing.
 * Use ::init_runtime_dfg instead
 */
void set_local_runtime(struct dfg_runtime *rt);

/**
 * Initializes the DFG as loaded from a JSON file, and sets
 * the global variables such that the DFG and runtime can be accessed
 * @param filename File from which to load the json DFG
 * @param runtime_id Idenitifer for this runtime
 * @return 0 on success, -1 on error
 */
int init_runtime_dfg(char *filename, int runtime_id);

/**
 * Gets the sockaddr associated with the global controller
 * @param addr Output parameter to be filled with controller's ip and port
 * @return 0 on success, -1 on error
 */
int controller_address(struct sockaddr_in *addr);

/**
 * @returns the ID of the local runtime
 */
int local_runtime_id();

/**
 * @returns the port on which the local runtime is listening
 */
int local_runtime_port();

/*
 * @returns the IP address on which the local runtime is listening
 */
uint32_t local_runtime_ip();

/**
 * @returns the DFG with which this runtime was instantiated
 */
struct dedos_dfg *get_dfg();

/**
 * Frees the runtime's static instance of the DFG
 */
void free_runtime_dfg();

#endif


