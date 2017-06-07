#include "stats.h"
#include "logging.h"
#include "control_protocol.h"
#include "dedos_statistics.h"
#include "communication.h"
#include <string.h>

int send_stats_to_controller() {
    struct stat_sample samples[ N_REPORTED_STAT_TYPES * MAX_STAT_ITEM_IDS ];
    int sample_index = 0;
    for (int i=0; i< N_REPORTED_STAT_TYPES; i++) {
        enum stat_id stat_id = reported_stats[i];
        int n_stats = sample_stats(stat_id, STAT_DURATION_S, STAT_SAMPLE_SIZE,
                                   &samples[sample_index]);
        sample_index += n_stats;
    }

    struct stats_control_payload payload = {
        .n_samples = sample_index + 1,
        .samples = samples,
    };

    char buffer[MAX_STAT_BUFF_SIZE];

    int stat_buff_size = serialize_stat_payload(&payload, (void*)buffer);

    struct dedos_control_msg msg = {
        .msg_type = RESPONSE_MESSAGE,
        .msg_code = STATISTICS_UPDATE,
        .header_len = sizeof(msg),
        .payload_len = stat_buff_size,
        .payload = (void*)buffer
    };

    int rtn = send_control_msg(controller_sock, &msg);

    if ( rtn < 0 ) {
        log_error("Error sending statistics update to global controller");
        return -1;
    }

    return 0;
}
