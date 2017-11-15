#include "local_msu.h"
#include "msu_type.h"
#include "msu_message.h"
#include "msu_calls.h"
#include "logging.h"
#include "routing_strategies.h"

#include "webserver/httpops.h"
#include "webserver/write_msu.h"
#include "webserver/fileio_msu.h"
#include "webserver/cache_msu.h"
#include "webserver/connection-handler.h"
#include "connection-handler.h"

static int ws_fileio_load(struct local_msu *self,
                           struct msu_msg *msg) {
    struct response_state *resp = msg->data;
    resp->body_len = -1;

    FILE *file = fopen(resp->path, "rb");
    if (file == NULL) {
        log_info("Failed to access requested file %s with errno %d", resp->path, errno);
        resp->body_len = -1;
        resp->header_len = generate_header(resp->header, 404, MAX_HEADER_LEN, 0, NULL);
    } else {
        // Get file size
        fseek(file, 0, SEEK_END);
        unsigned int size = ftell(file);
        fseek(file, 0, SEEK_SET);
        if (size > MAX_BODY_LEN) {
            log_warn("File larger (%d) than body buffer (%d). Truncating contents.",
                     size, MAX_BODY_LEN);
            size = MAX_BODY_LEN;
        }

        // Allocate memory for file contents and populate it
        resp->body_len = fread(resp->body, sizeof(char), size, file);
        if (resp->body_len != size) {
            log_error("Only read %d of %d bytes from file %s", read, size, resp->path);
        }

        fclose(file);

        char *mimetype = path_to_mimetype(resp->path);
        resp->header_len = generate_header(resp->header, 200, MAX_HEADER_LEN, resp->body_len,
                                           mimetype);
    }

    call_msu_type(self, &WEBSERVER_CACHE_MSU_TYPE, &msg->hdr, sizeof(*resp), resp);
    call_msu_type(self, &WEBSERVER_WRITE_MSU_TYPE, &msg->hdr, sizeof(*resp), resp);

    return 0;
}

struct msu_type WEBSERVER_FILEIO_MSU_TYPE = {
        .name = "Webserver_fileio_msu",
        .id = WEBSERVER_FILEIO_MSU_TYPE_ID,
        .receive = ws_fileio_load,
        .route = shortest_queue_route
};