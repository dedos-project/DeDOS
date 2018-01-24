/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
#include "dedos.h"
#include "logging.h"

#include "webserver/httpops.h"

#define BASE_HTTP_HEADER\
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
        return snprintf(dest, capacity, BASE_HTTP_HEADER, mime_type, body_len);
    } else if (code == 404) {
        return snprintf(dest, capacity, NOT_FOUND_HEADER);
    } else {
        return snprintf(dest, capacity, NOT_IMPLEMENTED_HEADER);
    }
}
