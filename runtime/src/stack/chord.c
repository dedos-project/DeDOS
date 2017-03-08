#include "chord.h"
#include "finger_table.h"
#include "hash_function.h"
#include "logging.h"

static int get_first_empty_slot_nodes_list(struct chord_ring* chord_ring){
    int i = 0;
    for (i = 0; i < MAX_CHORD_RING_NODES; i++) {
        if(chord_ring->nodes[i] == NULL){
            return i;
        }
    }
    return -1;
}

/*
static int key_in_range(unsigned int check, unsigned int bound1, unsigned int bound2, int half) {
    // log_debug("Checking if %u in (%u, %u]",check, bound1, bound2);
  if (bound1 > bound2) {
    if (half) {
      if (check <= bound2 || check > bound1) {
        return 1;
      }
    }
    else {
      if (check < bound2 || check > bound1) {
        return 1;
      }
    }
  }
  else {
    if (half) {
      if (check > bound1 && check <= bound2) {
        return 1;
      }
    }
    else {
      if (check > bound1 && check < bound2) {
        return 1;
      }
    }
  }

  // half enclosed range
  if (half) {
    // per III.C of the paper range is (a, b] in find_successor()
    return (check > bound1 && check <= bound2);
  }
  else {
    return (check > bound1 && check < bound2);
  }
}
*/

static int key_in_range(unsigned int check, unsigned int bound1, unsigned int bound2, int closed_upper_bound){
    if(bound1 < bound2){ //normal val check
        if(closed_upper_bound){
            if(bound1 < check && check <= bound2){
                return TRUE;
            }
            else{
                return FALSE;
            }
        }
        else{
            if(bound1 < check && check < bound2){
                return TRUE;
            }
            else{
                return FALSE;
            }
        }
    }
    else if(bound1 > bound2){ //hash val crosses the boundary
        if(closed_upper_bound){
            if(bound1 < check && check <= MAX_KEY){
                return TRUE;
            }else if(check <= bound2 && check >= MIN_KEY){
                return TRUE;
            }
            else{
                return FALSE;
            }
        }
        else{
            if(bound1 < check && check <= MAX_KEY){
                return TRUE;
            }else if(check < bound2 && check >= MIN_KEY){
                return TRUE;
            }
            else{
                return FALSE;
            }
        }
    }
    else { //bounds are equal
        // log_warn("Key bounds are equal: %u, %u",bound1, bound2);
        if(check == bound1){
            return TRUE;
        }
    }
    return FALSE;
}

void init_chord_ring_in_place(struct chord_ring *chord_ring){
    int i;
    chord_ring->first_node = NULL;
    chord_ring->last_node = NULL;
    chord_ring->size = 0;
    for (int i=0; i<MAX_CHORD_RING_NODES; i++){
        chord_ring->nodes[i] = NULL;
    }
    log_debug("Initialized chord ring%s", "");
}

struct chord_ring *init_chord_ring(void){
    struct chord_ring *chord_ring;
    int i;
    chord_ring = (struct chord_ring*)malloc(sizeof(struct chord_ring));
    if(!chord_ring){
        log_error("Failed to malloc chord ring%s","");
        return NULL;
    }
    init_chord_ring_in_place(chord_ring); 
    return chord_ring;
}

void empty_chord_ring(struct chord_ring* chord_ring){
    int i;
    struct chord_node *cur_node;

    if(chord_ring->size > 0){
        //Free all the nodes first and then delete the ring
        log_debug("Need to remove %u nodes for empty_chord_ring", chord_ring->size);
        for (i = 0; i < MAX_CHORD_RING_NODES; i++) {
            cur_node = chord_ring->nodes[i];
            if(cur_node != NULL){
                node_leave(chord_ring, cur_node);
                destroy_chord_node(chord_ring, cur_node);
                ring_stabilise_all(chord_ring);
            }
        }
    }
}

void destroy_chord_ring(struct chord_ring* ring){
    int i;
    struct chord_node *cur_node;

    if(ring->size > 0){
        //Free all the nodes first and then delete the ring
        log_debug("Need to free %u nodes first before destroy_chord_ring", ring->size);
        for (i = 0; i < MAX_CHORD_RING_NODES; i++) {
            cur_node = ring->nodes[i];
            if(cur_node != NULL){
                destroy_chord_node(ring, cur_node);
            }
        }
    }
    free(ring);
}

struct chord_node* init_chord_node(struct chord_ring* chord_ring, struct msu_endpoint* endpoint, int next_msu_id, unsigned int next_msu_type){

    struct chord_node *new_node = NULL;
    int add_index = -1;
    int i;

    add_index = get_first_empty_slot_nodes_list(chord_ring);
    if(add_index == -1 || chord_ring->size == MAX_CHORD_RING_NODES){
        log_error("MAX_CHORD_RING_NODES reached, cannot add more node: %d",chord_ring->size);
        return NULL;
    }

    new_node = malloc(sizeof(struct chord_node));
    if(!new_node){
        log_error("Failed to malloc new node %s","");
        return NULL;
    }

    new_node->next_msu_id = next_msu_id;
    new_node->next_msu_type = next_msu_type;
    new_node->key = compute_hash(next_msu_id);
    if(new_node->key == 0){
        log_warn("returned chord key is 0, could be error %s","");
    }
    // new_node->key_hex = (char*)malloc(sizeof(unsigned char)* SHA_DIGEST_LENGTH * 2);
    // if(!new_node->key_hex){
    //     log_error("failed to malloc key_hex %s","");
    //     free(new_node);
    //     return NULL;
    // }
    // memset(new_node->key_hex, 0x0, SHA_DIGEST_LENGTH*2);
    // for (i=0; i < SHA_DIGEST_LENGTH; i++) {
    //     sprintf(&(new_node->key_hex[i*2]), "%02x", new_node->key[i]);
    // }
    new_node->finger_table = init_finger_table(new_node);
    new_node->state = NODE_STATE_RUNNING;
    new_node->index_in_array = add_index;
    new_node->msu_endpoint = endpoint;

    chord_ring->nodes[add_index] = new_node;
    chord_ring->size++;

    log_debug("Initialized chord node index: %d, id: %d, key: %u",add_index, next_msu_id, new_node->key);
    return new_node;
}

void destroy_chord_node(struct chord_ring* chord_ring, struct chord_node* node){
    int clear_index;
    int i;

    if(node == NULL){
        return;
    }
    clear_index = node->index_in_array;
    destroy_finger_table(node);
    // free(node->key);
    // free(node->key_hex);

    chord_ring->nodes[clear_index] = NULL;
    chord_ring->size--;

    //check if this was the only node left
    if(chord_ring->size == 0){
        chord_ring->first_node = NULL;
        chord_ring->last_node = NULL;
    }

    //fix first and last if this node was either or both of them
    if(node == chord_ring->first_node){
        log_warn("This node is first node of ring: %d",node->next_msu_id);
        if(node->successor != NULL){
            chord_ring->first_node = node->successor;
        }
        else{
            log_warn("Assigning random node as first node%s","");
            for (i = 0; i < MAX_CHORD_RING_NODES; i++) {
                if(chord_ring->nodes[i] != NULL){
                    chord_ring->first_node = chord_ring->nodes[i];
                    break;
                }
            }
        }
    }

    if(node == chord_ring->last_node){
        log_warn("This node is last node of ring: %d",node->next_msu_id);
        if(node->predecessor != NULL){
            chord_ring->last_node = node->predecessor;
        }
        else{
            log_warn("Assigning random node as last node%s","");
            for (i = MAX_CHORD_RING_NODES - 1; i >= 0; i--) {
                if(chord_ring->nodes[i] != NULL){
                    chord_ring->last_node = chord_ring->nodes[i];
                    break;
                }
            }
        }
    }

    free(node);
    log_debug("Destroyed chord node at index: %d",clear_index);
}

void print_all_nodes(struct chord_ring *chord_ring){
    int i;
    struct chord_node* cur_node;
    log_debug("Current total nodes: %d",chord_ring->size);

    printf("-------------------------------------------------------------\n");
    printf("------------------- Nodes in Array ------------------------------\n");
    printf("%-8s\t%-8s\t%-15s\t%-15s\n", "MSU_Id", "Key", "Pred_key(ID)", "Succ_key(ID)");
    printf("-------------------------------------------------------------\n");

    for (i = 0; i < MAX_CHORD_RING_NODES; i++) {
        cur_node = chord_ring->nodes[i];
        if(cur_node != NULL){
            // printf("Index: %d, Id: %d, Key: %u\n", i, cur_node->next_msu_id, cur_node->key);
            print_node(cur_node, FALSE);
        }
    }
}

int node_join(struct chord_ring *chord_ring, struct chord_node* intro_node, struct chord_node* new_node){
    if(chord_ring->first_node == NULL){ //first node
        chord_ring->first_node = new_node;
        chord_ring->last_node = new_node;
        new_node->predecessor = NULL;
        new_node->successor = new_node;
        // log_debug("Added first node to ring%s","");
    }
    else{ //there are some existing nodes
        // log_debug("Adding via first node%s","");
        if(intro_node == NULL){ //use the first node as intro by default
            intro_node = chord_ring->first_node;
        }
        new_node->successor = node_find_successor(intro_node, new_node->key);
        if(new_node->successor == NULL){
            log_error("Unable to find successor node for node %d, key: %s",new_node->next_msu_id, new_node->key_hex);
            return -1;
        }

        /* update chord ring if necessary */
        if (new_node->key < chord_ring->first_node->key) {
          chord_ring->first_node = new_node;
        }
        if (new_node->key > chord_ring->last_node->key) {
          chord_ring->last_node = new_node;
        }
    }

    return 0;
}

void node_leave(struct chord_ring *chord_ring, struct chord_node* node){

    struct chord_node *my_predecessor, *my_successor;
    if(node == NULL){
        return;
    }
    my_predecessor = node->predecessor;
    my_successor = node->successor;
    my_successor->predecessor = my_predecessor;
    my_predecessor->successor = my_successor;
}

struct chord_node* node_find_successor(struct chord_node* intro_node, unsigned int key){
    return do_node_find_successor(intro_node, intro_node, key, 0);
}

struct chord_node* node_find_closest_preceding_node(struct chord_node *node, unsigned int key){
    int i;
    struct finger *finger = NULL;

    for (i = KEY_BITS - 1; i >= 0; i--) {
        finger = node->finger_table->fingers[i];

        if (key_in_range(finger->start, node->key, key, 0)) {
            return finger->node;
        }
    }
    return node;
}

struct chord_node* do_node_find_successor(struct chord_node* orig_node, struct chord_node* node, unsigned int key, int depth){
    struct chord_node *closest_preceding_node = NULL;

    depth++;

    if (depth > KEY_BITS * 2) {
        return node_find_successor(orig_node->successor, key);
    }

    if (key_in_range(key, node->key, node->successor->key, TRUE) || node == node->successor) {
        // log_debug("In base case %s","");
        return node->successor;
    }
    else {
        closest_preceding_node = node_find_closest_preceding_node(node, key);
        if (closest_preceding_node == node) {
            return do_node_find_successor(orig_node, node->successor, key, depth);
        }
        return do_node_find_successor(orig_node, closest_preceding_node, key, depth);
    }
}

void node_stabilize(struct chord_node *node) {
    struct chord_node *successor = node->successor;
    struct chord_node *x = node->successor->predecessor;
    int i = 0;

    if (x != NULL) {
        if (node == node->successor || key_in_range(x->key, node->key, node->successor->key, FALSE)) {
          node->successor = x;
        }
    }
    node_notify(node->successor, node);
}

void node_notify(struct chord_node *notify_node, struct chord_node *check_node){
    if ((notify_node->predecessor == NULL
       || key_in_range(check_node->key, notify_node->predecessor->key, notify_node->key, 0))) {
        /* check_node thinks it might be notify_node's predecessor */
        notify_node->predecessor = check_node;
    }
}

void ring_stabilise_all(struct chord_ring* chord_ring){
    struct chord_node *current = NULL;
    int i = 0;
    int done_first = 0;

    for (i = 0; i < MAX_CHORD_RING_NODES; i++) {
        current = chord_ring->nodes[i];
        if(current != NULL){
            node_stabilize(current);
            node_fix_fingers(current);
        }
    }
}

void print_node(struct chord_node* cur, int print_finger_table){
    log_debug("%-8d\t%-8u\t%u(%d)\t%u(%d)\t%-6d\n", cur->next_msu_id, cur->key,
                cur->predecessor != NULL ? cur->predecessor->key : -1,
                cur->predecessor != NULL ? cur->predecessor->next_msu_id : -1,
                cur->successor != NULL ? cur->successor->key : -1,
                cur->successor != NULL ? cur->successor->next_msu_id : -1,
                cur->index_in_array);
    if(print_finger_table){
        node_print_finger_table(cur);
    }
}

void traverse_ring(struct chord_ring* chord_ring, int direction){
    struct chord_node* cur;
    log_debug("----------------------------------------------------------%s---","");
    log_debug("------------------- Chord Ring: %s -----------------------------", direction == DIR_CLOCKWISE ? "CLOCKWISE" : "COUNTER-CLOCKWISE");
    if(chord_ring == NULL || (chord_ring->first_node == NULL && chord_ring->size == 0)){
        log_debug("----- Empty chord ring -----%s","");
        return;
    }
    log_debug("%-8s\t%-8s\t%-15s\t%-15s\t%-15s\n", "MSU_Id", "Key", "Pred_key(ID)", "Succ_key(ID)","Array_Index");
    log_debug("-----------%s--------------------------------------------------","");

    cur = chord_ring->first_node;
    do {
        print_node(cur, FALSE);
        if(direction == DIR_CLOCKWISE){
            cur = cur->successor;
        }
        else{
            cur = cur->predecessor;
        }
    } while (cur != chord_ring->first_node);
}
