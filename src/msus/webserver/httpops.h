#ifndef HTTPOPS_H
#define HTTPOPS_H

char *path_to_mimetype(char *path);

int url_to_path(char *url, char *dir, char *path, int capacity);

int generate_header(char *dest, int code, int capacity, int body_len, char *mime_type);

#endif //HTTPOPS_H
