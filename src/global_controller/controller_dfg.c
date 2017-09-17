
static struct dedos_dfg *DFG = NULL;

int init_controller_dfg(char *filename) {

    if (DFG != NULL) {
        log_error("Controller DFG already instantiated");
        return -1;
    }

    DFG = parse_dfg_json_file(filename);
    if (DFG == NULL) {
        log_error("Error initializing DFG");
        return -1;
    }

    return 0;
}

struct dedos_dfg *get_dfg() {
    return DFG;
}
