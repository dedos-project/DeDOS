#ifndef RUNTIME_COMMUNICATION_H__
#define RUNTIME_COMMUNICATION_H__

int send_to_runtime(unsigned int runtime_id, struct ctrl_runtime_msg_hdr *hdr, 
                    void *payload);

int handle_runtime_communication(int fd);

#endif
