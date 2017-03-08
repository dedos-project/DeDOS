#ifndef CHORD_H_
#define CHORD_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "routing.h"

#define KEY_BITS 8 * sizeof(unsigned int)
#define MAX_KEY pow(2, KEY_BITS)
#define MIN_KEY 0
#define MAX_CHORD_RING_NODES 200
#define NODE_STATE_RUNNING 1
#define NODE_STATE_DEAD 2
#define DIR_CLOCKWISE 1
#define DIR_COUNTER_CLOCKWISE 2

#define TRUE 1
#define FALSE 0

struct chord_node;
struct finger;
struct finger_table;

struct finger {
    struct chord_node *node;
    int start;
};

struct finger_table {
    struct finger **fingers;
    int length;
};

struct chord_node {
    int next_msu_id;
    unsigned int next_msu_type;
    unsigned int key; //derived from next_msu_id
    char *key_hex; //derived from next_msu_id
    struct chord_node *predecessor;
    struct chord_node *successor;
    struct finger_table *finger_table;
    int state;
    int index_in_array;
    struct msu_endpoint *msu_endpoint;
};

struct chord_ring {
    struct chord_node *first_node;
    struct chord_node *last_node;
    unsigned size;
    struct chord_node *nodes[MAX_CHORD_RING_NODES];
};

/* Ring functions */
struct chord_ring* init_chord_ring(void);
void destroy_chord_ring(struct chord_ring* ring);
void empty_chord_ring(struct chord_ring* ring);

/* Chord node functions */
struct chord_node* init_chord_node(struct chord_ring* chord_ring, struct msu_endpoint* endpoint, int next_msu_id, unsigned int next_msu_type);
void destroy_chord_node(struct chord_ring* chord_ring, struct chord_node* node);
int node_join(struct chord_ring *chord_ring, struct chord_node* intro_node, struct chord_node* new_node);
void node_leave(struct chord_ring *chord_ring, struct chord_node* node);

struct chord_node* node_find_successor(struct chord_node* intro_node, unsigned int key);
struct chord_node* do_node_find_successor(struct chord_node* orig_node, struct chord_node* tmp_node, unsigned int key, int depth);
struct chord_node* node_find_closest_preceding_node(struct chord_node *node, unsigned int key);

void node_stabilize(struct chord_node* node);
void node_notify(struct chord_node *notify_node, struct chord_node *check_node);
void ring_stabilise_all(struct chord_ring *chord_ring);

struct finger_table* init_finger_table(struct chord_node* chord_node);
void destroy_finger_table(struct chord_node* chord_node);
void print_node(struct chord_node* cur, int print_finger_table);
void print_all_nodes(struct chord_ring *chord_ring);
void traverse_ring(struct chord_ring* chord_ring, int direction);

#endif
