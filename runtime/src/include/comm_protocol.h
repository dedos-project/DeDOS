#ifndef COMM_PROTOCOL_H_
#define COMM_PROTOCOL_H_

#include "data_plane_profiling.h"
//Anything that is received over the TCP socket between
//runtimes on different machines should have this structure

typedef struct dedos_intermsu_message {
    int src_msu_id;
    int dst_msu_id;
    unsigned int proto_msg_type;
    unsigned int payload_len;
#ifdef DATAPLANE_PROFILING
    unsigned long int payload_request_id;
    int payload_seq_count;
#endif
    void *payload;
} intermsu_msg;


#endif /* COMM_PROTOCOL_H_ */
