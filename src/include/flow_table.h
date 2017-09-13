#include "generic_msu.h"
#include "logging.h"
#include "chord.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "finger_table.h"
#include "hash_function.h"

#define MIN(a,b) ((a)>(b)?(b):(a))
#define MAX(a,b) ((a)>(b)?(a):(b))

struct chord_node* add_chord_node(struct chord_ring* chord_ring, struct msu_endpoint* endpoint, int msu_id, unsigned int msu_type);
void remove_chord_node(struct chord_ring *chord_ring, struct chord_node* node);
struct chord_node* lookup_key(struct chord_ring* ring, unsigned int key);
struct chord_node* lookup_node(struct chord_ring* chord_ring, int msu_id);
int update_flow_table(struct generic_msu* self, struct msu_control_update *update_msg);
