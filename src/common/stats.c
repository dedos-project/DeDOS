/**
 * @file stats.c
 *
 * Functions for the sending and receiving of statsitics between ctrl and runtime
 */
#include "stats.h"
#include "logging.h"

#include <stdlib.h>

/**
 * Header for a serialized stats message.
 *
 * Comes before a set of samples (i.e. the whole stats message)
 */
struct stat_msg_hdr {
    /** The number of *items* sampled (not the size of the sample) */
    int n_samples;
};

int is_thread_stat(enum stat_id id) {
    for (int i=0; i < N_REPORTED_THREAD_STAT_TYPES; i++) {
        if (reported_thread_stat_types[i].id == id) {
            return 1;
        }
    }
    return 0;
}

int is_msu_stat(enum stat_id id) {
    for (int i=0; i < N_REPORTED_MSU_STAT_TYPES; i++) {
        if (reported_msu_stat_types[i].id == id) {
            return 1;
        }
    }
    return 0;
}



/** Initializes a single stat sample with room to hold `max_stats` statistics */
static int init_stat_sample(int max_stats, struct stat_sample *sample) {
    sample->max_stats = max_stats;
    sample->stats = calloc(max_stats, sizeof(*sample->stats));
    if (sample->stats == NULL) {
        log_error("Error allocating sample stats");
        return -1;
    }
    return 0;
}

void free_stat_samples(struct stat_sample *sample, int n_samples) {
    if (sample != NULL) {
        for (int i=0; i<n_samples; i++) {
            free(sample[i].stats);
        }
        free(sample);
    }
}

struct stat_sample *init_stat_samples(int max_stats, int n_samples) {
    struct stat_sample *sample = calloc(n_samples, sizeof(*sample));
    if (sample == NULL) {
        log_error("Could not allocate %d stat samples", n_samples);
        return NULL;
    }
    for (int i=0; i<n_samples; i++) {
        if (init_stat_sample(max_stats, &sample[i]) != 0) {
            log_error("Error initializing stat sample %d", i);
            free(sample);
            return NULL;
        }
    }
    return sample;
}

size_t serialized_stat_sample_size(struct stat_sample *sample, int n_samples) {
    size_t size = sizeof(sample->hdr) * n_samples;
    for (int i=0; i<n_samples; i++) {
        size += sizeof(*sample->stats) * sample[i].hdr.n_stats;
    }
    size += sizeof(struct stat_msg_hdr);
    return size;
}

/** Serializes a single stat sample into a buffer of size `buff_len`.
 * @return 0 on success, -1 if buffer is not large enough to hold sample */
static ssize_t serialize_stat_sample(struct stat_sample *sample, void *buffer, size_t buff_len) {
    if (buff_len < sizeof(sample->hdr) + sizeof(*sample->stats) * sample->hdr.n_stats) {
        log_error("Buffer isn't large enough to fit serialized stat sample");
        return -1;
    }

    char *curr_buff = buffer;
    memcpy(curr_buff, &sample->hdr, sizeof(sample->hdr));
    curr_buff += sizeof(sample->hdr);
    size_t stat_size = sizeof(*sample->stats) * sample->hdr.n_stats;
    memcpy(curr_buff, sample->stats, stat_size);

    return stat_size + sizeof(sample->hdr);
}

ssize_t serialize_stat_samples(struct stat_sample *samples, int n_samples,
                               void *buffer, size_t buff_len) {
    if (buff_len < serialized_stat_sample_size(samples, n_samples)) {
        log_error("Buffer isn't large enough to fit serialized stat samples");
        return -1;
    }

    struct stat_msg_hdr hdr = { .n_samples = n_samples };
    memcpy(buffer, &hdr, sizeof(hdr));
    hdr.n_samples= n_samples;

    char *curr_buff = ((char*)buffer) + sizeof(hdr);
    char *end_buff = curr_buff + buff_len;

    for (int i=0; i<n_samples; i++) {
        size_t new_size = serialize_stat_sample(&samples[i], curr_buff, end_buff-curr_buff);
        if (new_size < 0) {
            log_error("Erorr serializing stat sample %d", i);
            return -1;
        }
        curr_buff += new_size;
    }

    log(LOG_STAT_SERIALIZATION, "Serialized %d samples into buffer of size %d",
               n_samples, (int)(curr_buff - (char*)buffer));
    return curr_buff - (char*)buffer;
}

/** Deserializes a single stat sample from a buffer
 * @returns 0 on success, -1 on error
 */
static ssize_t deserialize_stat_sample(void *buffer, size_t buff_len, struct stat_sample *sample) {
    if (buff_len < sizeof(sample->hdr)) {
        log_error("Buffer isn't large enough to fit header");
        return -1;
    }

    char *curr_buff = buffer;
    memcpy(&sample->hdr, buffer, sizeof(sample->hdr));
    curr_buff += sizeof(sample->hdr);

    size_t stat_size = sizeof(*sample->stats) * sample->hdr.n_stats;
    if (sizeof(sample->hdr) + stat_size > buff_len) {
        log_error("Buffer isn't large enough to fit statistics");
        return -1;
    }
    if (sample->max_stats < sample->hdr.n_stats) {
        log_error("Not enough statistics allocated to fit deserialized stats");
        return -1;
    }
    memcpy(sample->stats, curr_buff, stat_size);
    return sizeof(sample->hdr) + stat_size;
}

int deserialize_stat_samples(void *buffer, size_t buff_len, struct stat_sample *samples,
                                 int n_samples) {
    struct stat_msg_hdr *hdr = buffer;
    if (buff_len < sizeof(*hdr)) {
        log_error("Buffer not large enough for stat message header");
        return -1;
    }
    if (n_samples < hdr->n_samples) {
        log_error("Number of allocated samples smaller than in serialized buffer");
        return -1;
    }

    char *curr_buff = ((char *)buffer) + sizeof(*hdr);
    char *end_buff = curr_buff + buff_len;
    for (int i=0; i < hdr->n_samples; i++) {
        ssize_t consumed = deserialize_stat_sample(curr_buff, end_buff - curr_buff, &samples[i]);
        if (consumed < 0) {
            log_error("Error deserializing stat sample %d", i);
            return -1;
        }
        curr_buff += consumed;
    }

    log(LOG_STAT_SERIALIZATION, "Deserialized buffer of size %d into %d samples",
               ((int)(curr_buff - (char*) buffer)), hdr->n_samples);

    if (curr_buff - (char*)buffer < buff_len) {
        log_warn("Entire buffer not used when deserializing stats");
    }
    return hdr->n_samples;
}
