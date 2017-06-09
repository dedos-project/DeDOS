/**
 * dedos_statistics.h
 *
 * Contains definitions related to the communication of statistics betweeen the runtime
 * and the global controller
 */

#ifndef _DEDOS_STATISTICS_H_
#define _DEDOS_STATISTICS_H_
#include "control_protocol.h"
#include "logging.h"
#include "uthash.h"
#include "stats.h"

/**
 * These statistics are the ones that will be reported to the global controller.
 * The __attribute__ flag disables the warning about unused variables
 */
static enum stat_id reported_stats[] __attribute__((unused)) = {
    QUEUE_LEN, ITEMS_PROCESSED
};

/** The number of statistic types that are reported to the global controller. */
#define N_REPORTED_STAT_TYPES sizeof(reported_stats) / sizeof(enum stat_id)

/** The frequency in seconds with which statistics are reported to the global controller. */
#define STAT_DURATION_S  1
/** The number of statistics that are reported to the global controller every duration*/
#define STAT_SAMPLE_SIZE 5

/** The maximum number of samples the runtime might report to the global controller
 * in a single message. */
#define MAX_REPORTED_SAMPLES MAX_STAT_SAMPLES * N_REPORTED_STAT_TYPES

/** The maximum size that the buffer might be to hold all of the reported statistics */
#define MAX_STAT_BUFF_SIZE \
    N_REPORTED_STAT_TYPES * MAX_STAT_ITEM_IDS * \
    (sizeof(struct stat_sample) + sizeof(struct timed_stat) * MAX_STAT_SAMPLES) + \
    sizeof(struct stats_control_payload)

/** Serializes a stats payload into a buffer. */
int serialize_stat_payload(struct stats_control_payload *payload, void *buffer);
/** Deserializes a serialized buffer into a stats payload. */
int deserialize_stat_payload(void *payload, struct stats_control_payload *stats);

#endif
