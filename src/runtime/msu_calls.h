/**
 * @file msu_calls.h
 * Declares the methods available for calling an MSU from another MSU.
 * Note: All functions in this file which receive a `key` make a COPY
 * of the key, and as such it can have local scope.
 */

#ifndef MSU_CALLS_H_
#define MSU_CALLS_H_

#include "local_msu.h"
#include "msu_message.h"
#include "routing.h"

/** Enqueues a new message in the queue of a local MSU. */
int init_call_local_msu(struct local_msu *sender, struct local_msu *dest,
                        struct msu_msg_key *key, size_t data_size, void *data);

/** Enqueues a message in the queue of a local MSU. */
int call_local_msu(struct local_msu *sender, struct local_msu *dest,
                   struct msu_msg_hdr *hdr, size_t data_size, void *data);

/**
 * Sends a new MSU message to a destination of the given type,
 * utilizing the sending MSU's routing function.
 */
int init_call_msu_type(struct local_msu *sender, struct msu_type *dst_type,
                       struct msu_msg_key *key, size_t data_size, void *data);

/**
 * Sends an MSU message to a destination of the given type,
 * utilizing the sending MSU's routing function.
 */
int call_msu_type(struct local_msu *sender, struct msu_type *dst_type,
                  struct msu_msg_hdr *hdr, size_t data_size, void *data);

/** Sends a new MSU message to a specific destination, either local or remote.*/
int init_call_msu_endpoint(struct local_msu *sender, struct msu_endpoint *endpoint,
                           struct msu_type *endpoint_type,
                           struct msu_msg_key *key, size_t data_size, void *data);

/* Sends an MSU message to a speicific destination, either local or remote. */
int call_msu_endpoint(struct local_msu *sender, struct msu_endpoint *endpoint,
                      struct msu_type *endpoint_type,
                      struct msu_msg_hdr *hdr, size_t data_size, void *data);

#endif
