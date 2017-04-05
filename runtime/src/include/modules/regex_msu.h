#ifndef REGEX_MSU_H
#define REGEX_MSU_H
#include "ssl_msu.h"
#include "generic_msu.h"

#define MAX_REGEX_VALUE_LEN 64

/** Holds data to be delivered to a regex MSU */
struct regex_data_payload {
    char to_match[MAX_REGEX_VALUE_LEN];  /**< Regular expression to match */
    char string[MAX_REGEX_VALUE_LEN];        /**< String to match regex against */
    char http_response[MAX_REQUEST_LEN];
    int dst_type;      /**< Type of MSU destination */
    void *dst_packet; /**< Packet to be delivered to MSU destination */
};


const struct msu_type REGEX_MSU_TYPE;

#endif
