#include "connection-handler.h"
#include "dfg.h"
#include "local_msu.h"
#include "logging.h"
#include "msu_calls.h"
#include "msu_message.h"
#include "msu_type.h"
#include "routing_strategies.h"
#include "rt_stats.h"
#include "local_files.h"

#include "webserver/uthash.h"
#include "webserver/write_msu.h"
#include "webserver/cache_msu.h"
#include "webserver/fileio_msu.h"
#include "webserver/connection-handler.h"
#include "webserver/httpops.h"

#include <string.h>

#define DEFAULT_WWW_DIR "www/"
#define DEFAULT_OCCUPANCY_RATE 0.2
#define DEFAULT_MAX_KB_SIZE UINT_MAX
#define DEFAULT_MAX_FILES UINT_MAX
#define CACHE_INIT_SYNTAX "<www_dir>, <max_cache_size_in_kb>, <max_cached_files>, " \
                          "<max_cache_occupancy_rate>"

#define MONITOR_CACHE_STATS
#define CACHE_HIT_STAT MSU_STAT1
#define CACHE_MISS_STAT MSU_STAT2
#define CACHE_EVICT_STAT MSU_STAT3


static struct local_msu *cache_instance;

struct cached_file {
    long byte_size;
    char *path;
    char *contents;
    struct cached_file *lru_prev;
    struct cached_file *lru_next;
    UT_hash_handle hh;
};

struct ws_cache_state {
    unsigned int max_files;
    unsigned int max_kb_size;
    float max_occupancy_rate;
    unsigned long byte_size;
    unsigned int file_count;
    char *www_dir;
    struct cached_file *cache;
    struct cached_file *lru_head; // Contains the least recently used item
    struct cached_file *lru_tail; // Contains the last item used
};

struct cached_file *check_cache(struct ws_cache_state *fc, char *path) {
    struct cached_file *cached = NULL;
    HASH_FIND_STR(fc->cache, path, cached);
    if (cached != NULL) {
        log_info("File %s retrieved from cache", path);

        // Update LRU if not already the tail
        if (fc->lru_tail != cached) {
            if (fc->lru_head == cached) {
                fc->lru_head = cached->lru_next;
            } else {
                cached->lru_prev->lru_next = cached->lru_next;
            }
            cached->lru_next->lru_prev = cached->lru_prev;
            cached->lru_prev = fc->lru_tail;
            fc->lru_tail->lru_next = cached;
            cached->lru_next = NULL;
            fc->lru_tail = cached;
        }

        return cached;
    }

    return NULL;
}

static int parse_init_cache_payload(char *to_parse, struct ws_cache_state *cache_state) {

    cache_state->www_dir = DEFAULT_WWW_DIR;
    cache_state->max_kb_size = DEFAULT_MAX_KB_SIZE;
    cache_state->max_files = DEFAULT_MAX_FILES;
    cache_state->max_occupancy_rate = DEFAULT_OCCUPANCY_RATE;

    if (to_parse == NULL) {
        log_warn("Initializing cache MSU with default parameters. "
                         "(FYI: init syntax is [" CACHE_INIT_SYNTAX "])");
    } else {
        char *saveptr, *tok;
        if ( (tok = strtok_r(to_parse, " ,", &saveptr)) == NULL) {
            log_warn("Couldn't get wwW_dir from initialization string");
            return 0;
        }
        cache_state->www_dir = malloc(32 + strlen(tok));
        get_local_file(cache_state->www_dir, tok);

        if ( (tok = strtok_r(NULL, " ,", &saveptr)) == NULL) {
            log_warn("Couldn't get max cache size in kb from initialization string");
            return 0;
        }
        int max_kb_size = atoi(tok);
        if (max_kb_size >= 0)
            cache_state->max_kb_size = (unsigned int) max_kb_size;

        if ( (tok = strtok_r(NULL, " ,", &saveptr)) == NULL) {
            log_warn("Couldn't get max cached files from initialization string");
            return 0;
        }
        int max_files = atoi(tok);
        if (max_files >= 0)
            cache_state->max_files = (unsigned int) max_files;

        if ( (tok = strtok_r(NULL, " ,", &saveptr)) == NULL) {
            log_warn("Couldn't get max occupancy rate in kb from initialization string");
            return 0;
        }
        float max_occupancy_rate = atof(tok);
        if (max_occupancy_rate >= 0.0)
            cache_state->max_occupancy_rate = max_occupancy_rate;

        if ( (tok = strtok_r(NULL, " ,", &saveptr)) != NULL) {
            log_warn("Discarding extra tokens from cache initialization: %s", tok);
        }
    }
    return 0;
}

static int cache_file(struct ws_cache_state *fc, char *path, char *contents, long length) {
    // Only cache the file if it isn't too large
    float kbytes = (float) length / 1024;
    if (kbytes > fc->max_kb_size || kbytes / fc->max_kb_size > fc->max_occupancy_rate) {
        log_info("File at %s is too large for caching (%ld bytes)", path, length);
        return -1;
    }

    // Evict files if necessar11y
    while (((float) fc->byte_size + length) / 1024 > fc->max_kb_size ||
           fc->max_files == fc->file_count) {
        struct cached_file *cached = fc->lru_head;
        if (cached == NULL) {
            log_error("Trying to evict lru head that is NULL!");
            return -2;
        }

        log_info("Evicting %s from cache", cached->path);
#ifdef MONITOR_CACHE_STATS
        increment_stat(CACHE_EVICT_STAT, cache_instance->id, 1);
#endif

        HASH_DEL(fc->cache, cached);
        fc->lru_head = cached->lru_next;
        if (fc->lru_head != NULL) {
            fc->lru_head->lru_prev = NULL;
        }
        if (fc->lru_tail == cached) {
            fc->lru_tail = NULL;
        }
        fc->file_count--;
        fc->byte_size -= cached->byte_size;
        free(cached->path);
        free(cached->contents);
        free(cached);
    }

    // Now add the file to the cache
    struct cached_file *cached = (struct cached_file *) malloc(
            sizeof(struct cached_file));
    if (cached == NULL) {
        log_error("Failed to allocate space for cached_file struct for file %s", path);
        return -2;
    }

    log_info("Adding file %s to cache", path);

    // Add to cache
    cached->byte_size = length;
    // Copy path
    int path_len = strlen(path);
    cached->path = (char *) malloc(path_len + 1);
    strncpy(cached->path, path, path_len);
    cached->path[path_len] = '\0';
    // Copy contents
    if (length > 0) {
        cached->contents = (char *) malloc(length);
        memcpy(cached->contents, contents, length);
    } else {
        cached->contents = NULL;
    }
    fc->file_count++;
    HASH_ADD_STR(fc->cache, path, cached);

    // Update LRU linked list
    cached->lru_prev = fc->lru_tail;
    if (fc->lru_tail != NULL) {
        fc->lru_tail->lru_next = cached;
    }
    cached->lru_next = NULL;
    if (fc->lru_head == NULL) {
        fc->lru_head = cached;
    }
    fc->lru_tail = cached;
    fc->byte_size += length;

    return length;
}

static int ws_cache_lookup(struct local_msu *self,
                           struct msu_msg *msg) {
    struct response_state *resp = msg->data;
    struct ws_cache_state *fc = self->msu_state;

    url_to_path(resp->url, fc->www_dir, resp->path, MAX_FILEPATH_LEN);

    // TODO: Check message type properly to determine if this is a response to cache, or request
    if (resp->body[0] == '\0' && resp->body_len == 0) {
        struct cached_file *file = check_cache(fc, resp->path);
        if (file == NULL) {
            // File not cached, send to file IO msu
#ifdef MONITOR_CACHE_STATS
            increment_stat(CACHE_MISS_STAT, cache_instance->id, 1);
#endif
            call_msu_type(self, &WEBSERVER_FILEIO_MSU_TYPE, &msg->hdr, sizeof(*resp), resp);
        } else {
            // File cached, generate response including http headers and send to write msu
            int code = 404;
            char *mime_type = NULL;
            if (file->contents != NULL && file->byte_size > 0) {
                code = 200;
                resp->body_len = file->byte_size;
                mime_type = path_to_mimetype(resp->path);
                memcpy(resp->body, file->contents, resp->body_len);
            }
            resp->header_len = generate_header(resp->header, code, MAX_HEADER_LEN, resp->body_len,
                                               mime_type);
            if (resp->header_len > MAX_HEADER_LEN) {
                resp->header_len = MAX_HEADER_LEN;
            }
#ifdef MONITOR_CACHE_STATS
            increment_stat(CACHE_HIT_STAT, cache_instance->id, 1);
#endif
            call_msu_type(self, &WEBSERVER_WRITE_MSU_TYPE, &msg->hdr, sizeof(*resp), resp);
        }
    } else {
        // Received a response to save to the cache
        cache_file((struct ws_cache_state *)self->msu_state, resp->path, resp->body,
                   resp->body_len);
    }

    return 0;
}

static int ws_cache_init(struct local_msu *self, struct msu_init_data *init_data) {
    struct ws_cache_state *cache_state = malloc(sizeof(*cache_state));

    parse_init_cache_payload(init_data->init_data, cache_state);

    cache_state->byte_size = 0;
    cache_state->file_count = 0;
    cache_state->cache = NULL;
    cache_state->lru_head = NULL;
    cache_state->lru_tail = NULL;

    self->msu_state = (void*)cache_state;

    cache_instance = self;

#ifdef MONITOR_CACHE_STATS
    init_stat_item(CACHE_HIT_STAT, self->id);
    init_stat_item(CACHE_MISS_STAT, self->id);
    init_stat_item(CACHE_EVICT_STAT, self->id);
#endif

    return 0;
}

struct msu_type WEBSERVER_CACHE_MSU_TYPE = {
        .name = "Webserver_cache_msu",
        .id = WEBSERVER_CACHE_MSU_TYPE_ID,
        .receive = ws_cache_lookup,
        .init = ws_cache_init,
        .route = shortest_queue_route
};
