#ifndef MSU_STATE_H_
#define MSU_STATE_H_
#include "msu_message.h"

// Forward declaration to avoid circular dependency
struct local_msu;

int msu_num_states(struct local_msu *msu);

void *msu_init_state(struct local_msu *msu, struct msu_msg_key *key, size_t size);

void *msu_get_state(struct local_msu *msu, struct msu_msg_key *key, size_t *size);

int msu_free_state(struct local_msu *msu, struct msu_msg_key *key);

#endif
