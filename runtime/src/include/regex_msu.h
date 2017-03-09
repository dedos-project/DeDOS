#ifndef REGEX_MSU_H
#define REGEX_MSU_H

#define REGEX_MSU_ID 505

#define MAX_REGEX_VALUE_LEN 128
#define MAX_REQUEST_LEN 1024
#define REGEX_KEY "regex="
#define EVIL_REGEX "^(a+)+$"
#define HTML "\
<!DOCTYPE html>\n\
<html>\n\
    <body>\n\
        <h1>Does %s match %s?</h1> <br/>\n\
        <p>%s.</p>\n\
    </body>\n\
</html>\
"



struct regex_data_payload {
    char to_match[MAX_REGEX_VALUE_LEN];
    char string[MAX_REQUEST_LEN];
    char http_response[MAX_REQUEST_LEN];
    int dst_type;
    void *dst_packet;
};

#include "generic_msu.h"

const msu_type_t REGEX_MSU_TYPE;

#endif
