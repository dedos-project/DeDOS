#include "logging.h"
#include "control_protocol.h"
#include "dedos_statistics.h"
#include <string.h>


int serialize_stat_payload(struct stats_control_payload *payload, void *buffer) {
    void *buff_loc = buffer;
    memcpy(buff_loc, payload, sizeof(*payload));
    buff_loc += sizeof(*payload);
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
    return buff_loc - buffer;
}

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

    return offset_buffer - buffer;
}
