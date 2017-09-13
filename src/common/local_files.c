#include <stdio.h>

static char *local_dir;

int set_local_directory(char *dir) {
    local_dir = dir;
    return 0;
}

int get_local_file(char *out, char *file) {
    sprintf(out, "%s/%s", local_dir, file);
    return 0;
}
