#pragma once
#include <stdlib.h>
#include <stdio.h>
#include "jsmn.h"

/**
 * Asserts that the type of the current token is the provided type,
 * and logs an error if that is not the case
 */
#define ASSERT_JSMN_TYPE(tok, toktype, j) \
    if ((tok)->type != toktype){ \
        log_error("Got type %d, expected %d while parsing %.50s", (tok)->type, toktype, &j[(tok)->start]); \
        return -1; \
    } \

/**
 * Structure to hold state while parsing JSON
 */
struct json_state {
    void *data;
    int parent_type;
    struct json_state *next;
    jsmntok_t *tok;
};

/**
 * Typedef for a jsmn parsing function. Each element in the key mapping should
 * have one of these as the third element.
 *
 * @param tok - current token to be parsed
 * @param j   - entire JSON string
 * @param state - current state being parsed
 * @param saved - items that have been deferred to parse until later are stored here
 * @return 0 on success, -1 on error, 1 to defer parsing until a future pass
 */
typedef int (*jsmn_parsing_fn)(jsmntok_t **tok, char *j, struct json_state *state,
                               struct json_state **saved);

/**
 * Structure to map a key + state to a function
 */
struct key_mapping {
    char *key;              /**< The string in the JSON file */
    int parent_type;        /**< The current type, as set by a previous parsing fn.
                                 Starts at 0*/
    jsmn_parsing_fn parse;  /**< The function to be called when this string is seen
                                 at the appropriate place in the input JSON */
};

/**
 * Using the provided functions, parses the JSON present in
 * 'filename' and stores the resulting object in 'obj'.
 */
int parse_into_obj(const char *filename, void *obj, struct key_mapping *km);

/**
 * Can be used as a jsmn_parsing_fn that ignores any value passed into it
 */
int jsmn_ignore(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved);

/**
 * Typedef for a json_state initializer, used when iterating over lists of objects.
 *
 * @param state - current state being parsed
 * @param index - index into the current list
 * @return a new struct with the new data item of interest
 */
typedef struct json_state (*jsmn_initializer)(struct json_state *state, int index);


/** Parses a single JSMN object utilizing the key mapping provided in parse_into_obj() */
int parse_jsmn_obj(jsmntok_t **tok, char *j, struct json_state *in,
                          struct json_state **saved);

/** Parses an array of JSMN objects. */
int parse_jsmn_obj_list(jsmntok_t **tok, char *j, struct json_state *in,
                               struct json_state **saved, jsmn_initializer init);

/** Returns the initial object passed into the JSON parser */
void *get_jsmn_obj();

/** Destructively extracts a c-string from a jsmn token */
char *tok_to_str(jsmntok_t *tok, char *j);

/** Destructively extracts an int from a jsmn token. */
int tok_to_int(jsmntok_t *tok, char *j);

/** Destructively extracts an int from a jsmn token. */
long tok_to_long(jsmntok_t *tok, char *j);


