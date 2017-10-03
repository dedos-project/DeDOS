#include "webserver/connection-handler.h"
#include "logging.h"

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

static int url_callback(http_parser *parser, const char *at, size_t length) {
    struct parser_state *state = parser->data;
    strncpy(&state->url[state->url_len], at, length);
    state->url[state->url_len + length] = '\0';
    state->url_len += length;
    log(LOG_HTTP_PARSING, "Got URL: %s", state->url);
    return 0;
}

static int headers_complete_callback(http_parser *parser) {
    struct parser_state *state = parser->data;
    state->headers_complete = 1;
    log(LOG_HTTP_PARSING, "Got end of headers");
    return 0;
}

static int header_field_or_value_callback(http_parser UNUSED *parser, 
                                          const char UNUSED *at, 
                                          size_t UNUSED length) {
#if LOG_HTTP_PARSING
    char cpy[length+1];
    strncpy(cpy, at, length);
    cpy[length] = '\0'
    log(LOG_HTTP_PARSING, "Got header field %s", cpy);
#endif
    return 0;
}


void init_parser_state(struct parser_state *state) {
    memset(state, 0, sizeof(*state));
    state->settings.on_url = url_callback;
    state->settings.on_header_field = header_field_or_value_callback;
    state->settings.on_header_value = header_field_or_value_callback;
    state->settings.on_headers_complete = headers_complete_callback;
    http_parser_init(&state->parser, HTTP_REQUEST);
    state->parser.data = (void*)state;
}

int parse_http(struct parser_state *state, char *buf, ssize_t bytes) {
    if (state == NULL) {
        log_error("Cannot handle connection with NULL state");
        return WS_ERROR;
    }

    if (state->headers_complete) {
        log_warn("Parsing even though header is already complete");
    }

    size_t nparsed = http_parser_execute(&state->parser, &state->settings, 
                                         buf, bytes);

    if (nparsed != bytes) {
        buf[bytes] = '\0';
        log_error("Error parsing HTTP request %s (rcvd: %d, parsed: %d)",
                buf, (int)bytes, (int)nparsed);
        return WS_ERROR;
    }

    return state->headers_complete && state->url_len ? WS_COMPLETE : WS_INCOMPLETE_READ;
}

