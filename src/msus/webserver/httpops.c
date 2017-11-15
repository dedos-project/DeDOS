#include "logging.h"

#include "webserver/httpops.h"

#define DEFAULT_HTTP_HEADER\
    "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n"

#define NOT_FOUND_HEADER\
    "HTTP/1.1 404 Not Found\r\n\r\n"

#define NOT_IMPLEMENTED_HEADER\
    "HTTP/1.1 501 Not Implemented\r\n\r\n"

#define DEFAULT_MIME_TYPE\
    "text/html"

char *path_to_mimetype(char *path) {
    char *extension = path;
    for (; *extension != '.' && *extension != '\0'; extension++);

    if (*extension == '\0')
        return DEFAULT_MIME_TYPE;

    extension++; // Advance past the dot

    // TODO: Necessary to replace this by loading /etc/mime.types into hashmap?
    if (strcasecmp(extension, "html") == 0 ||
        strcasecmp(extension, "htm") == 0 ||
        strcasecmp(extension, "txt") == 0) {
        return "text/html";
    } else if (strcasecmp(extension, "png") == 0) {
        return "image/png";
    } else if (strcasecmp(extension, "jpg") == 0 ||
               strcasecmp(extension, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(extension, "gif") == 0) {
        return "image/gif";
    }

    return DEFAULT_MIME_TYPE;
}

int url_to_path(char *url, char *dir, char *path, int capacity) {
    int len = 0;
    for (char *c = url; *c != '\0' && *c != '?'; c++, len++);

    int dir_len = strlen(dir);
    int dir_slash = (dir[dir_len - 1] == '/');
    int url_slash = (url[0] == '/');
    dir_len -= (dir_slash && url_slash);
    int both_missing_slash = (!dir_slash && !url_slash);

    if (dir_len + len + both_missing_slash >= capacity) {
        log_error("Path from url (%s) too large for buffer (%d)", url, capacity);
        return -1;
    }

    char *dest = path;

    strncpy(dest, dir, dir_len);
    if (both_missing_slash) {
        dest[dir_len] = '/';
    }
    strncpy(&dest[dir_len + both_missing_slash], url, len);
    dest[dir_len + both_missing_slash + len] = '\0';
    return dir_len + len;
}

int generate_header(char *dest, int code, int capacity, int body_len, char *mime_type) {
    if (code == 200) {
        return snprintf(dest, capacity, DEFAULT_HTTP_HEADER, mime_type, body_len);
    } else if (code == 404) {
        return snprintf(dest, capacity, NOT_FOUND_HEADER);
    } else {
        return snprintf(dest, capacity, NOT_IMPLEMENTED_HEADER);
    }
}