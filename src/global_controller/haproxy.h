#ifndef HAPROXY_H
#define HAPROXY_H

int reweight_haproxy(int server, int weight);

void set_haproxy_weights(int rt_id, int offset);
#endif

