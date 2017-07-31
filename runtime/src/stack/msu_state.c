#include "msu_state.h"
#include "stats.h"
#include "generic_msu.h"
#include <stdlib.h>

#ifndef LOG_MSU_STATE_MANAGEMENT
#define LOG_MSU_STATE_MANAGEMENT 0
#endif

/**
 *  Hidden data structure to protect access to MSU specific data.
 *  Data should be added to this structure via:
 *      msu_init_state()
 *  accessed via:
 *      msu_get_state()
 *  and freed via:
 *      msu_free_state()
 */
struct msu_state{
    uint32_t id;
    size_t size;
    void *data;
    struct msu_state *substates;
    int n_substates;
    UT_hash_handle hh;
};

#define CHECK_STATE_REPLACEMENT 0
static inline int alloc_substates(struct msu_state *parent, unsigned int n) {
    if (parent->n_substates < n ) {
        int orig = parent->n_substates;
        size_t new_size = (n - orig) * sizeof(*parent->substates);
        parent->size += new_size;
        parent->n_substates = n;
        parent->substates = realloc(parent->substates, sizeof(*parent->substates) * n);
        if (parent->substates == NULL) {
            log_error("error reallocating substates");
            return -1;
        }
        memset(&parent->substates[orig], 0, sizeof(parent->substates) * (n - orig));
        // Apparently 0 != (size_t)0, so this has to be set separately? 
        // Or else I'm doing something wrong...
        for (int i=orig; i<n; i++) {
            parent->substates[i].size = 0;
        }
        log_custom(LOG_MSU_STATE_MANAGEMENT, "Allocated %d new substates (from %d), size: %d",
                   (int)(n - orig), orig, (int)new_size);
        return (int)new_size;
    }
    return 0;
}
/**
 * Initializes and returns the state assiciated with a given key.
 * Will NOT do good things if the key is already associated with another state for the same MSU,
 * so use `msu_get_state()` to check for key existance first.
 * @param msu MSU which will contain the state of interest
 * @param key key for the state to be entered
 * @param size size of the data to allocate
 * @return void* pointer to the newly allocated data
 */
void *msu_init_state(struct generic_msu *msu, uint32_t key, unsigned int key2, size_t size) {
    struct msu_state *state_p = malloc(sizeof(*state_p));
    state_p->id = key;
    state_p->data = malloc(size);
    state_p->size = size;
    if (key2 == 0) {
        log_custom(LOG_MSU_STATE_MANAGEMENT,
                "Allocating new state of size %d for msu %d, key %d-%d",
                (int)size, msu->id, key, key2);
        state_p->n_substates = 0;
#if CHECK_STATE_REPLACEMENT
        struct msu_state *old_data = NULL;
        HASH_REPLACE(hh, msu->state_p, id, sizeof(key), state_p, old_data);
        if (old_data != NULL) {
            log_warn("Replacing old state! Not freeing! This could be very bad!");
        }
#else
        HASH_ADD(hh, msu->state_p, id, sizeof(key), state_p);
#endif
        increment_stat(MEMORY_ALLOCATED, msu->id, (double)(sizeof(*state_p) + size));
        log_custom(LOG_MSU_STATE_MANAGEMENT,
                   "returning state %p for key %d", state_p->data, (int)key);
        return state_p->data;
    } else {
        log_custom(LOG_MSU_STATE_MANAGEMENT,
                "Allocating new substate of size %d for msu %d, key %d-%u",
                (int)size, msu->id, key, key2);
        size_t added_size = 0;
        struct msu_state *parent;
        HASH_FIND(hh, msu->state_p, &key, sizeof(key), parent);
        if (parent == NULL) {
            parent = malloc(sizeof(*parent));
            parent->id = key;
            parent->size = 0;
            added_size += sizeof(*parent);
            parent->substates = NULL;
            parent->n_substates = 0;
            HASH_ADD(hh, msu->state_p, id, sizeof(key), parent);
        }
        int new_size = alloc_substates(parent, key2);
        if (new_size < 0) {
            return NULL;
        }
        added_size += new_size;
        struct msu_state *substate = &parent->substates[key2-1];
        int orig_size = substate->size;
        log_custom(LOG_MSU_STATE_MANAGEMENT, "orig substate size: %d", orig_size);
        substate->size = size;
        parent->size += size - orig_size;
        log_custom(LOG_MSU_STATE_MANAGEMENT, "Adding %d to state allocation (now: %d)",
                   (int)(size + new_size - orig_size), (int)parent->size);
        added_size += size;
        substate->data = realloc(substate->data, size);
        log_custom(LOG_MSU_STATE_MANAGEMENT, "Added allocation: %d", (int)added_size);
        increment_stat(MEMORY_ALLOCATED, msu->id, (double)added_size);
        return substate->data;
    }
}

/**
 * Reallocates the state associated with a given key for an MSU.
 * If the state does not exist, this will create it.
 * @param msu MSU containing the state
 * @param key key for the state to be changed
 * @param size size fo the data to realloc
 * @return void* pointer to the newly realloc'd data
 */
void *msu_realloc_state(struct generic_msu *msu, uint32_t key, unsigned int key2, size_t size) {
    struct msu_state *state_p;
    HASH_FIND(hh, msu->state_p, &key, sizeof(key), state_p);
    if (state_p == NULL) {
        log_custom(LOG_MSU_STATE_MANAGEMENT, 
                "Reallocation of msu %d, key%d-%u actually initialization",
                msu->id, key, key2);
        return msu_init_state(msu, key, key2, size);
    } else {
        log_custom(LOG_MSU_STATE_MANAGEMENT,
                "Reallocating msu %d, key%d-%u, to size %d", msu->id, key, key2, (int)size);
        if (key2 == 0) {
            state_p->data = realloc(state_p->data, size);
            if (state_p->data == NULL) {
                state_p->size = 0;
                log_warn("Realloc of state for msu %d failed", msu->id);
            } else {
                state_p->size = size;
            }
            return state_p->data;
        } else {
            int added_from_substates = alloc_substates(state_p, key2);
            if (added_from_substates < 0) {
                return NULL;
            }
            size_t added_size = (size_t) added_from_substates;
            struct msu_state *substate = &state_p->substates[key2-1];
            int orig_subsize = substate->size;
            substate->size = size;
            state_p->size += size + added_size - orig_subsize;
            substate->data = realloc(substate->data, size);
            increment_stat(MEMORY_ALLOCATED, msu->id, (double)(size - orig_subsize));
            return substate->data;
        }
    }
}

/**
 * Gets the state associated with a given key for a given MSU.
 * @param msu The MSU which contains the state associated with this key 
 * @param key The key used to retrieve the state
 * @param *size Used as an output parameter to store the size of the retrieved state. 
 *              Pass NULL if size is not of interest
 * @return void* pointer to the allocated state or NULL if key doesn't exist
 */
void *msu_get_state(struct generic_msu *msu, uint32_t key, unsigned int key2, size_t *size) {
    struct msu_state *state_p;
    HASH_FIND(hh, msu->state_p, &key, sizeof(key), state_p);
    log_custom(LOG_MSU_STATE_MANAGEMENT,
            "Accessing state for MSU %d, key %d-%u", msu->id, key, key2);
    if (state_p == NULL) {
        log_custom(LOG_MSU_STATE_MANAGEMENT, "State does not exist!");
        return NULL;
    } else {
        if (key2 == 0) {
            if (size != NULL)
                *size = state_p->size;
            log_custom(LOG_MSU_STATE_MANAGEMENT, 
                       "returning state %p for key %d", state_p->data, (int)key);
            return state_p->data;
        } else {
            if (state_p->n_substates < key2) {
                return NULL;
            }
            if (size != NULL)
                *size = state_p->substates[key2-1].size;
            return state_p->substates[key2-1].data;
        }
    }
}

/**
 * Frees the state associated with a given key for a given MSU
 * @param msu the MSU which contains the state associated with this key
 * @param key The key used to retrieve the state
 * @return 0 on success, -1 on error
 */
int msu_free_state(struct generic_msu *msu, uint32_t key, unsigned int key2) {
    struct msu_state *state_p;
    HASH_FIND(hh, msu->state_p, &key, sizeof(key), state_p);;
    log_custom(LOG_MSU_STATE_MANAGEMENT,
               "Freeing state for MSU %d, key %d-%u", msu->id, key, key2);
    if (state_p == NULL) {
        log_custom(LOG_MSU_STATE_MANAGEMENT,
                   "State does not exist!");
        return -1;
    } else {
        if (key2 == 0) {
            for (int i=0; i<state_p->n_substates; i++) {
                free(state_p->substates[i].data);
            }
            if (state_p->n_substates) 
                free(state_p->substates);
            HASH_DEL(msu->state_p, state_p);
            log_custom(LOG_MSU_STATE_MANAGEMENT,
                    "freeing state containing allocation of size %d (total: %d)", 
                    (int)state_p->size, (int)(sizeof(*state_p) + state_p->size));
            increment_stat(MEMORY_ALLOCATED, msu->id,
                           (double)(-1 * (int)(sizeof(*state_p) + state_p->size)));
            free(state_p->data);
            free(state_p);
            return 0;
        } else {
            if (key2 >= state_p->n_substates) {
                return -1;
            }
            free(state_p->substates[key2].data);
            state_p->substates[key2].data = NULL;
            int orig_size = state_p->substates[key2].size;
            state_p->substates[key2].size = 0;
            increment_stat(MEMORY_ALLOCATED, msu->id, (double)(-1 * orig_size));
            return 0;
        }
    }
}

void msu_free_all_state(struct generic_msu *msu) {
    // Iterate over and free all of the states
    struct msu_state *state, *tmp;
    HASH_ITER(hh, msu->state_p, state, tmp) {
        log_info("Freeing MSU %d state %d due to deletion", msu->id, state->id);
        msu_free_state(msu, state->id, 0);
    }
}
