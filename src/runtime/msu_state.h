/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania

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
 * @file msu_state.h
 *
 * State storage that is tied to a specific MSU mesasge.
 *
 * States can be stored or retrieved by passing an msu_msg_key.
 */

#ifndef MSU_STATE_H_
#define MSU_STATE_H_
#include "msu_message.h"

// Forward declaration to avoid circular dependency
struct local_msu;

/** @returns the number of states allocated by the provided MSU */
int msu_num_states(struct local_msu *msu);

/**
 * Initializes a new MSU state of the given size with the provided key.
 * The returned pointer should later be freed with ::msu_free_state
 * @returns a pointer to the newly allocated state, or NULL if error
 */
void *msu_init_state(struct local_msu *msu, struct msu_msg_key *key, size_t size);

/**
 * Gets the state allocated with the given key
 * @param msu The MSU storing the state
 * @param key The key with which the state was allocated
 * @param size An output argument, set to the size of the retrieved state if non-NULL
 * @return Allocated state if it exists, otherwise NULL
 */
void *msu_get_state(struct local_msu *msu, struct msu_msg_key *key, size_t *size);

/** Frees the state assocated with the given MSU and key */
int msu_free_state(struct local_msu *msu, struct msu_msg_key *key);

/** Frees all state structures associated with the given MSU */
int msu_free_all_state(struct local_msu *msu);
#endif
