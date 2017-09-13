#ifndef FINGER_TABLE_H_
#define FINGER_TABLE_H_

#include <math.h>
#include "chord.h"

struct finger* init_finger(struct chord_node* node, int start);
struct finger* destroy_finger(struct finger* finger);

struct finger_table* init_finger_table(struct chord_node* node);
void destroy_finger_table(struct chord_node* node);

void node_fix_fingers(struct chord_node *node);
void node_print_finger_table(struct chord_node *node);

#endif
