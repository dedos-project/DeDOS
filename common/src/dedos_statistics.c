/**
 * dedos_statistics.c
 *
 * Contains functions that relate to the reporting of statistics from the runtime to the
 * global controller. Namely, serialization.
 */
#include "logging.h"
#include "control_protocol.h"
#include "dedos_statistics.h"
#include <string.h>

#ifndef LOG_DEDOS_STATISTICS
/** Custom log level */
#define LOG_DEDOS_STATISTICS 0
#endif

/** Serialize s a stats payload into a buffer.
 * @param payload The full statistics payload to serialize
 * @param buffer The buffer into which to serialize the payload. Must be large enough to
 * contain the serialized payload. Recommended size: MAX_STAT_BUFF_SIZE
 * @return Size of the allocated buffer on success, -1 on error
 */
int serialize_stat_payload(struct stats_control_payload *payload, void *buffer) {
    void *buff_loc = buffer;
    memcpy(buff_loc, payload, sizeof(*payload));
    buff_loc += sizeof(*payload);
    // TODO: I think this can all be done in one memcpy
    for ( int sample_i=0; sample_i<payload->n_samples; sample_i++ ) {
        struct stat_sample *sample = &payload->samples[sample_i];
        memcpy(buff_loc, sample, sizeof(*sample));
        buff_loc += sizeof(*sample);
    }
    for ( int sample_i=0; sample_i<payload->n_samples; sample_i++ ) {
        struct stat_sample *sample = &payload->samples[sample_i];
        for ( int stat_i=0; stat_i < sample->n_stats; stat_i++ ) {
            struct timed_stat *stat = &sample->stats[stat_i];
            memcpy(buff_loc, stat, sizeof(*stat));
            buff_loc += sizeof(*stat);
        }
    }
    log_custom(LOG_DEDOS_STATISTICS, "Serialized stat payload into buffer of length %d",
               (int)(buff_loc-buffer));
    return (int)(buff_loc - buffer);
}

/** Deserializes a serialized buffer into a stats payload.
 * NOTE: This function does NOT allocate the payload NOR does it do a deep copy of the buffer
 * into the payload. It simply points the internal payload pointers to the relevant bits of the
 * buffer. Do not attempt to access the payload after freeing the buffer.
 * @param buffer The buffer into which the payload has already be serialized
 * @param payload The struct into which the payload is to be placed
 * @return The size of the buffer that was used on success, -1 on error
 *
 */
int deserialize_stat_payload(void *buffer, struct stats_control_payload *payload){

    void *offset_buffer = buffer;
    *payload = *(struct stats_control_payload *)buffer;
    offset_buffer += sizeof(*payload);

    if ( payload->n_samples > MAX_REPORTED_SAMPLES ) {
        log_error("Payload sample size must be less than %ld (actual: %d)",
                  MAX_REPORTED_SAMPLES, payload->n_samples);
        return -1;
    }

    payload->samples = (struct stat_sample *)offset_buffer;
    offset_buffer += sizeof(struct stat_sample) * payload->n_samples;
    for (int i=0; i<payload->n_samples; i++) {
        payload->samples[i].stats = (struct timed_stat *)offset_buffer;
        offset_buffer += sizeof(struct timed_stat) * payload->samples[i].n_stats;
    }
    log_custom(LOG_DEDOS_STATISTICS, "Deserialized stat payload from buffer of length %d",
               (int)(offset_buffer-buffer));

    return (int)(offset_buffer - buffer);
}
