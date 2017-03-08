#include "chord.h"
#include "finger_table.h"
#include "logging.h"

struct finger* init_finger(struct chord_node *node, int start) {
    struct finger *finger = NULL;

    finger = malloc(sizeof(struct finger));
    if(!finger){
        log_error("Failed to malloc finger%s","");
        return NULL;
    }
    finger->node = node;
    finger->start = start;

    return finger;
}

struct finger_table* init_finger_table(struct chord_node* node){
    int i;
    struct finger *finger = NULL;
    struct finger_table *finger_table = NULL;
    unsigned int start;

    finger_table = malloc(sizeof(struct finger_table));
    if(!finger_table){
        log_error("Failed to malloc finger table %s","");
        return NULL;
    }

    finger_table->length = KEY_BITS;

    finger_table->fingers = malloc(sizeof(struct finger) * finger_table->length);
    if(!finger_table->fingers){
        log_error("Failed to malloc memory for array of finger pointers%s","");
        free(finger_table);
        return NULL;
    }

    for (i = 0; i < finger_table->length; i++) {
        start = node->key + pow(2, i);
        if (start > MAX_KEY) {
            start -= MAX_KEY;
        }
        finger = init_finger(node, start);
        finger_table->fingers[i] = finger;
    }

    return finger_table;
}

void destroy_finger_table(struct chord_node* node){
    struct finger_table *finger_table;
    int i;

    finger_table = node->finger_table;
    for (i = 0; i < finger_table->length; i++) {
        free(finger_table->fingers[i]);
    }

    free(finger_table->fingers);
    free(finger_table);

    log_debug("Freed finger table for node: %d",node->next_msu_id);
}

void node_fix_fingers(struct chord_node *node){
    int i;
    struct finger *finger = NULL;
    struct chord_node *nodes[KEY_BITS];

    /* reset */
    for (i = 0; i < KEY_BITS; i++) {
        finger = node->finger_table->fingers[i];
        finger->node = node->successor;
    }

    for (i = 0; i < KEY_BITS; i++) {
        finger = node->finger_table->fingers[i];
        nodes[i] = node_find_successor(node, finger->start);
    }

    for (i = 0; i < KEY_BITS; i++) {
        finger = node->finger_table->fingers[i];
        finger->node = nodes[i];
    }
}

void node_print_finger_table(struct chord_node *node) {
    int i;
    struct finger *finger = NULL;

    printf("\n");
    printf("%-3s %-6s %-16s\n", "i", "Start", "Succ (ID:Key)");
    printf("--- ------ ----------------\n");

    for (i = 0; i < KEY_BITS; i++) {
        finger = node->finger_table->fingers[i];
        printf("%-3d %-6u %10d : %-4u\n", i, finger->start, finger->node->next_msu_id, finger->node->key);
    }
    printf("--- ------ ----------------\n");
}
