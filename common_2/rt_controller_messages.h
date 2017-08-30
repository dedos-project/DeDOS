#ifndef RT_CONTROLLER_MESSAGES_H_
#define RT_CONTROLLER_MESSAGES_H_

enum rt_controller_msg_type {
    RT_STATS
};

struct rt_controller_msg {
    enum rt_controller_msg_type type;
    size_t payload_size;
    // TODO: rt_controller_msg
};

#endif
