/**
 * @file msu_state.c
 *
 * State storage that is tied to a specific MSU message
 */
#include "msu_message.h"
#include "rt_stats.h"
#include "logging.h"
#include "uthash.h"
#include "local_msu.h"

/** Explicitly check whether a state is being replaced on a call to msu_init_state */
#define CHECK_STATE_REPLACEMENT 1

/** The structure contining msu state */
struct msu_state {
    /** The key with which the state is stored */
    struct composite_key key;
    /** The size of the data being stored */
    size_t size;
    /** An ID to facilitate the transferring of state between msus (not implemented yet) */
    int group_id;
    /** A unique identifier for the state 
     * (not used at the moment, will be utilized in state transfer) */
    int32_t id;
    /** The payload of the state */
    void *data;
    /** For use with the UT hash structure */
    UT_hash_handle hh;
};

int msu_num_states(struct local_msu *msu) {
    return HASH_COUNT(msu->item_state);
}

void *msu_init_state(struct local_msu *msu, struct msu_msg_key *key, size_t size) {
    struct msu_state *state = malloc(sizeof(*state));
    memcpy(&state->key, &key->key, key->key_len);
    state->group_id = key->group_id;
    state->data = malloc(size);
    state->size = size;
    state->id = key->id;

    log(LOG_STATE_MANAGEMENT, "Allocating new state of size %d for msu %d, key %d",
               (int)size, msu->id, (int)key->id);

#if CHECK_STATE_REPLACEMENT
    struct msu_state *old_state = NULL;
    HASH_REPLACE(hh, msu->item_state, key, key->key_len, state, old_state);
    if (old_state != NULL) {
        log_warn("Replacing old state! Not freeing! Bad!");
    }
#else
    HASH_ADD(hh, msu->item_state, key, key->key_len, state);
#endif

    increment_stat(MSU_MEM_ALLOC, msu->id, (double)(sizeof(*state) + size));
    increment_stat(MSU_NUM_STATES, msu->id, 1);

    return state->data;
}

void *msu_get_state(struct local_msu *msu, struct msu_msg_key *key, size_t *size) {
    struct msu_state *state;
    HASH_FIND(hh, msu->item_state, &key->key, key->key_len, state);
    log(LOG_STATE_MANAGEMENT, "Accessing state for MSU %d, key %d",
               msu->id, (int)key->id);
    if (state == NULL) {
        log(LOG_STATE_MANAGEMENT, "State does not exist");
        return NULL;
    }
    if (size != NULL) {
        *size = state->size;
    }
    return state->data;
}

int msu_free_state(struct local_msu *msu, struct msu_msg_key *key) {
    struct msu_state *state;
    HASH_FIND(hh, msu->item_state, &key->key, key->key_len, state);
    log(LOG_STATE_MANAGEMENT, "Freeing state for MSU %d, key %d",
               msu->id, (int)key->id);
    if (state == NULL) {
        log_warn("State for MSU %d, key %d does not exist",
                 msu->id, (int)key->id);
        return -1;
    }
    HASH_DEL(msu->item_state, state);

    increment_stat(MSU_MEM_ALLOC, msu->id,
                   (double)(-1 * (int)(sizeof(*state) + state->size)));
    increment_stat(MSU_NUM_STATES, msu->id, -1);
    free(state->data);
    free(state);
    return 0;
}

void msu_free_all_state(struct local_msu *msu) {
    struct msu_state *state, *tmp;
    HASH_ITER(hh, msu->item_state, state, tmp) {
        HASH_DEL(msu->item_state, state);
        free(state->data);
        free(state);
    }
}
