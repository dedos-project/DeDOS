#ifndef ROUTING_H_
#define ROUTING_H_

struct msu_endpoint {
    int id;
    enum locality locality;
    union {
        uint32_t ipv4;
        struct msg_queue *queue;
    }
}

/**
 * Forward declaration so reads and writes have to take place through API and adhere to lock
 */
struct routing_table;

struct route_reference {
    int id;
    struct routing_table *table;
};

#endif
