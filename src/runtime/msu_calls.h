#ifndef MSU_CALLS_H_
#define MSU_CALLS_H_

#include "local_msu.h"
#include "msu_message.h"
#include "routing.h"

int init_call_local_msu(struct local_msu *sender, struct local_msu *dest,
                        struct msu_msg_key *key, size_t data_size, void *data);

int call_local_msu(struct local_msu *sender, struct local_msu *dest,
                   struct msu_msg_hdr *hdr, size_t data_size, void *data);

int init_call_msu_type(struct local_msu *sender, struct msu_type *dst_type,
                       struct msu_msg_key *key, size_t data_size, void *data);

int call_msu_type(struct local_msu *sender, struct msu_type *dst_type,
                  struct msu_msg_hdr *hdr, size_t data_size, void *data);

int init_call_msu_endpoint(struct local_msu *sender, struct msu_endpoint *endpoint,
                           struct msu_type *endpoint_type,
                           struct msu_msg_key *key, size_t data_size, void *data);

int call_msu_endpoint(struct local_msu *sender, struct msu_endpoint *endpoint,
                      struct msu_type *endpoint_type,
                      struct msu_msg_hdr *hdr, size_t data_size, void *data);

#endif
