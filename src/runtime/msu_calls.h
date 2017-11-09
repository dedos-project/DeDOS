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
#include "logging.h"

/**
 * Enqueues a new message in the queue of a local MSU.
 * This function is identical to `call_local_msu()` with the exception that it
 * creates the required header from the specified message key. It is to be used
 * for initial enqueueing rather than existing message passsing
 *
 * @param sender The MSU sending the message
 * @param dest The MSU to receive the message
 * @param key The key to assign to the header of the new MSU message
 * @param data_size Size of the data to be enqueued to the next MSU
 * @param data the data to be enqueued to the next MSU
 * @return 0 on success, -1 on error
 */
int init_call_local_msu(struct local_msu *sender, struct local_msu *dest,
                        struct msu_msg_key *key, size_t data_size, void *data);

/** Enqueues a message in the queue of a local MSU.
 *
 * @param sender The MSU sending the message
 * @param dest The MSU to receive the message
 * @param hdr The header of the message received by the sender. To be modified
 *            with provinance and used in routing decisions
 * @param data_size Size of the data to be enqueued to the next MSU
 * @param data The data to be enqueued to the next MSU
 * @return 0 on success, -1 on error
 */
int call_local_msu(struct local_msu *sender, struct local_msu *dest,
                   struct msu_msg_hdr *hdr, size_t data_size, void *data);

/**
 * Sends an MSU message to a destination of the specified type.
 * This function is identical to ::call_msu_type with the exception that it
 * performs header initialization. To be used when performing an initial enqueue, rather
 * than continuing a chain of enqueues.
 * @param sender The MSU sending the message
 * @param dst_type The MSU type to receive the message
 * @param key The key to assign to the message header
 * @param data_size The size of the data to be enqueued
 * @param data The data to be enqueued
 * @return 0 on success, -1 on error
 */
int init_call_msu_type(struct local_msu *sender, struct msu_type *dst_type,
                       struct msu_msg_key *key, size_t data_size, void *data);

/**
 * Sends an MSU message to a destination of the given type,
 * utilizing the sending MSU's routing function.
 *
 * @param sender The MSU sending the message
 * @param dst_type The MSU type to receive the message
 * @param hdr The header to be used in routing, passed on from the sender's received message
 * @param data_size Size of the data to be enqueued
 * @param data The data to be enqueued
 * @return 0 on success, -1 on error
 */
int call_msu_type(struct local_msu *sender, struct msu_type *dst_type,
                  struct msu_msg_hdr *hdr, size_t data_size, void *data);


/**
 * Sends an MSU message to a specific destination, either local or remote.
 *
 * This function is identical to ::call_msu_endpoint with the exception that
 * it constructs the header for the message and is to be used for initial enqueues.
 * @param sender The MSU sending the message
 * @param endpoint The endpoint to which the message is to be delivered
 * @param endpoint_type The MSU type of the endpoint to receive the message
 * @param hdr The header of the MSU message received by the sender
 * @param data_size The size of the sending data
 * @param data The data sent
 * @return 0 on success, -1 on error
 */
int init_call_msu_endpoint(struct local_msu *sender, struct msu_endpoint *endpoint,
                           struct msu_type *endpoint_type,
                           struct msu_msg_key *key, size_t data_size, void *data);

/**
 * Sends an MSU message to a speicific destination, either local or remote.
 *
 * @param sender The MSU sending the message
 * @param endpoint The endpoint to which the message is to be delivered
 * @param endpoint_type The MSU type of the endpoint to receive the message
 * @param hdr The header of the MSU message received by the sender
 * @param data_size The size of the sending data
 * @param data The data sent
 * @return 0 on success, -1 on error
 */
int call_msu_endpoint(struct local_msu *sender, struct msu_endpoint *endpoint,
                      struct msu_type *endpoint_type,
                      struct msu_msg_hdr *hdr, size_t data_size, void *data);

/**
 * Schedules a call to a local MSU to occur at some point in the future.
 *
 * @param sender The MSU sending the message
 * @param dest The MSU to which the message should be delivered
 * @param interval The amount of time later at which the MSU should be called
 * @param hdr The header of the MSU messge received by the sender
 * @param data_size The size of the sending data
 * @param data the data sent
 * @return 0 on success, -1 on error
 */
int schedule_local_msu_call(struct local_msu *sender, struct local_msu *dest, struct timespec *interval,
                            struct msu_msg_hdr *hdr, size_t data_size, void *data);


/**
 * Schedules a call to a local MSU to occur at some point in the future.
 * Note: This function is identical to ::schedule_local_msu_call, but is meant for times
 * when the message flow is just beginning, and is to be used for initial enqueues.
 *
 * @param sender The MSU sending the message
 * @param dest The MSU to which the message should be delivered
 * @param interval The amount of time later at which the MSU should be called
 * @param hdr The header of the MSU messge received by the sender
 * @param data_size The size of the sending data
 * @param data the data sent
 * @return 0 on success, -1 on error
 */
int schedule_local_msu_init_call(struct local_msu *sender, struct local_msu *dest, struct timespec *interval,
                      struct msu_msg_key *key, size_t data_size, void *data);
#endif
