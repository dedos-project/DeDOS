#include "msu_message.h"
#include "rt_stats.h"
#include "logging.h"
#include "uthash.h"
#include "local_msu.h"

#define CHECK_STATE_REPLACEMENT 1

struct msu_state {
    struct composite_key key;
    size_t size;
    int group_id;
    int32_t id;
    void *data;
    UT_hash_handle hh;
};

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


