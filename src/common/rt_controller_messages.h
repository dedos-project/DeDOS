#ifndef RT_CONTROLLER_MESSAGES_H_
#define RT_CONTROLLER_MESSAGES_H_

enum rt_controller_msg_type {
    RT_CTL_INIT,
    RT_STATS
};

struct rt_controller_msg_hdr {
    enum rt_controller_msg_type type;
    size_t payload_size;
};

struct rt_controller_init_msg {
    unsigned int runtime_id;
    uint32_t ip;
    int port;
};

#endif
