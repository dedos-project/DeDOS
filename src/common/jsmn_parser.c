/**
 * @file jsmn_parser.c
 *
 * General-purpose function to interact with JSMN library, and create
 * objects (potentially with circular references) from the parsed
 * json.
 */

#include "jsmn.h"
#include "jsmn_parser.h"
#include "logging.h"
#include <string.h>
#include <strings.h>

/** Destructively extracts an int from a jsmn token.
 * Sets the "end" char to \0, and converts converts the resulting
 * string to an integer
 *
 * @param tok - JSMN token to extract
 * @param j   - original json string
 * @returns   - integer
 */
int tok_to_int(jsmntok_t *tok, char *j){
    j[tok->end] = '\0';
    return atoi(&j[tok->start]);
}

/** Destructively extracts an int from a jsmn token.
 * Sets the "end" char to \0, and converts converts the resulting
 * string to a long integer
 *
 * @param tok - JSMN token to extract
 * @param j   - original json string
 * @returns   - long
 */
long tok_to_long(jsmntok_t *tok, char *j){
    j[tok->end] = '\0';
    return atol(&j[tok->start]);
}


/** Destructively extracts a c-string from a jsmn token.
 * Sets the "end" char to \0, and returns a pointer
 * to the start
 *
 * @param tok - JSMN token to extract
 * @param j   - original json string
 * @returns   - null-terminated char *
 */
char *tok_to_str(jsmntok_t *tok, char *j){
    j[tok->end] = '\0';
    return &j[tok->start];
}

/**
 * Global key mapping, giving which function should be called for each JSMN key
 */
static struct key_mapping *jsmn_key_map;

/**
 * Retries tokens which have been deferred
 */
static int retry_saved(struct json_state **saved, char *j);

/**
 * The object which is pased into the JSON parser to be filled
 */
static void * global_obj;

/**
 * Returns the initial object passed into the JSON parser
 */
void *get_root_jsmn_obj(){
    return global_obj;
}

int jsmn_ignore_list(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    int size = (*tok)->size;
    log(LOG_JSMN_PARSING, "Ignoring list of size %d", size);
    for (int i=0; i<size; i++){
        ++(*tok); // Move to the next value
        jsmn_ignore(tok, j, in, saved);
    }
    return 0;
}

int jsmn_ignore_obj(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    int size = (*tok)->size;
    log(LOG_JSMN_PARSING, "Ignoring object of size %d", size);
    for (int i=0; i<size; i++) {
        ++(*tok); // Ignore the key
        ++(*tok); // Move to the next value
        jsmn_ignore(tok, j, in, saved);
    }
    return 0;
}

/**
 * Ignores a JSMN value
 */
int jsmn_ignore(jsmntok_t **tok, char *j, struct json_state *in, struct json_state **saved){
    switch ( (*tok)->type ) {
        case JSMN_OBJECT:
            jsmn_ignore_obj(tok, j, in, saved);
            break;
        case JSMN_ARRAY:
            jsmn_ignore_list(tok, j, in, saved);
            break;
        default:
            break;
    }
    return 0;
}


int parse_file_into_obj(const char *filename, void *obj, struct key_mapping *km){
    // Set the global key map
    jsmn_key_map = km;

    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        log_perror("Error opening %s", filename);
        return -1;
    }

    // Get the size of the file
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    rewind(file);

    // Read the file
    char *contents = malloc(fsize + 1);
    size_t rtn = fread(contents, fsize, 1, file);
    if (rtn == 0) {
        log_error("Could not read from file");
        return -1;
    }
    contents[fsize] = '\0';
    fclose(file);

    rtn = parse_str_into_obj(contents, obj, km);
    free(contents);
    return rtn;
}

/** The maximum number of times that a #PARSE_FN can return `1` before
 * an error is raised */
#define MAX_RETRIES 1024


int parse_str_into_obj(char *contents, void *obj, struct key_mapping *km) {

    // Initialize the JSON parser
    jsmn_parser parser;
    jsmn_init(&parser);
    // Get the number of necessary tokens
    int n_tokens = jsmn_parse(&parser, contents, strlen(contents), NULL, 16384);
    log(LOG_JSMN_PARSING, "Allocating %d tokens", n_tokens);
    jsmntok_t *tokens = malloc(n_tokens * sizeof(*tokens));
    jsmntok_t *tok_orig = tokens;
    // Parse the JSON
    jsmn_init(&parser);
    n_tokens = jsmn_parse(&parser, contents, strlen(contents), tokens, n_tokens);
    log(LOG_JSMN_PARSING, "Allocated %d tokens", n_tokens);

    // Set the top-level object so it can be retrieved elsewhere
    global_obj = obj;

    // Set the initial state for the first object
    // (type of 0 should indicate the root object)
    struct json_state init_state = {
        .parent_type = 0,
        .data = obj
    };
    struct json_state *saved_state = NULL;

    // Parse the jsmn tokens (assumes top-level item is an object)
    int rtn = parse_jsmn_obj(&tokens, contents, &init_state, &saved_state);

    if (rtn < 0){
        log_error("Error parsing JSMN");
        return -1;
    }

    log(LOG_JSMN_PARSING, "Parsed %d top-level objects", rtn);

    int n_retries = 0;
    // Go back through and retry each of the tokens that couldn't be parsed
    // the first time around
    while (saved_state != NULL){
        log(LOG_JSMN_PARSING, "Retrying saved states");
        rtn = retry_saved(&saved_state, contents);
        if (rtn < 0){
            log_error("Failed to re-interpret saved JSMN tokens");
            free(tok_orig);
            return -1;
        }
        n_retries++;
        if (n_retries > 100) {
            log_error("Something is wrong. Retried too many times");
            free(tok_orig);
            return -1;
        }
    }
    free(tok_orig);
    return 0;
}

/** Returns the function to parse a given key.
 *
 * @param key - the JSON key to parse
 * @param parent_type - the type of object in which this key is located
 * @return reference to function for parsing this key
 */
static jsmn_parsing_fn get_parse_fn(char *key, int parent_type){

    for (struct key_mapping *km = jsmn_key_map;
            km->parse != 0; km++){
        if (strncasecmp(key, km->key, strlen(km->key)) == 0 &&
                km->parent_type == parent_type){
            return km->parse;
        }
    }
    return NULL;
}

/**
 * Retries the states that failed to parse the first time
 * due to dependencies.
 *
 * @param saved_states - the states which failed to parse the first time
 *                       (returned 1)
 * @param j - the entire JSON string
 * @return 0 if at least one saved state was parsed, -1 otherwise
 */
static int retry_saved(struct json_state **saved_states, char *j){
    struct json_state *prev = NULL;
    struct json_state *saved = *saved_states;

    int success = -1;
    char *key = NULL;
    int n_retries = 0;
    while (saved != NULL){
        jsmntok_t *tok = saved->tok;
        key = tok_to_str(tok, j);
        log(LOG_JSMN_PARSING, "Retrying key: %s", key);

        jsmn_parsing_fn parser = get_parse_fn(key, saved->parent_type);
        tok += 1;
        int rtn = parser(&tok, j, saved, saved_states);

        if (rtn < 0){
            log_error("Error retrying key %s", key);
            return -1;
        } else if (rtn == 0){
            log(LOG_JSMN_PARSING, "Successfully parsed retried key '%s'", key);
            success = 0;
            if (*saved_states == saved)
                *saved_states = saved->next;
            if (prev != NULL) {
                prev->next = saved->next;
            }
            struct json_state *tmp = saved;
            saved = saved->next;
            free(tmp);
        } else if (rtn == 1){
            log(LOG_JSMN_PARSING, "Will try again on the next pass...");
            prev = saved;
            saved = saved->next;
        } else {
            log_error("Unknown return code: %d", rtn);
            return -1;
        }
        if (n_retries++ > MAX_RETRIES) {
            log_error("Retried too many times. Something is wrong");
            return -1;
        }
    }

    if (success == -1 && key != NULL) {
        log_error("Failed while reprocessing key '%s' for missing dependencies", key);
        log_error("Value for improper key %s", key + (strlen(key) + 1));
    }
    return success;
}

/** Parses an array of JSMN objects.
 * Calls init() for each new object, then passes the returned json_state
 * to the next parsed object.
 *
 * @param tok - the current jsmn token to parse
 * @param j   - the entire json string
 * @param state - the current state of parsing
 * @param saved - list of states to be re-interpreted laster
 * @param init  - initialization function, returning a `struct json_state`
 * @return 0 on success, -1 on failure
 */
int parse_jsmn_obj_list(jsmntok_t **tok, char *j, struct json_state *state,
                               struct json_state **saved, jsmn_initializer init){
    ASSERT_JSMN_TYPE(*tok, JSMN_ARRAY, j);
    int size = (*tok)->size;

    // Advance to the object inside the list
    ++(*tok);
    for (int i=0; i<size; i++){
        log(LOG_JSMN_PARSING, "Parsing list item %d", i);
        // This is only for lists of OBJECTS.
        ASSERT_JSMN_TYPE(*tok, JSMN_OBJECT, j);

        // Construct the new state to be passed to the next object
        struct json_state new_state =  init(state, i);
        new_state.tok = *tok;
        new_state.next = NULL;

        // Parse the next object in the list
        int n_tokens = parse_jsmn_obj(tok, j, &new_state, saved);

        // Advance to the subsequent object
        ++(*tok);
        if (n_tokens < 0){
            return -1;
        }
    }
    // parse_jsmn_obj handles advancing to the next object, so we
    // want to rewind here so as not to advance twice
    --(*tok);
    return 0;
}

/** Parses a single JSMN object utilizing the provided (global) key map.
 *
 * @param tok - pointer to current token to be parsed (advances automatically)
 * @param j   - entire json string being parsed
 * @param state - curernt state of parsing, including the data structure being parsed into
 * @param saved - JSON entries on which parsing has been deferred for later
 *
 */
int parse_jsmn_obj(jsmntok_t **tok, char *j, struct json_state *state,
                          struct json_state **saved){
    ASSERT_JSMN_TYPE(*tok, JSMN_OBJECT, j);

    int size = (*tok)->size;

    ++(*tok);
    //tpok+=1;
    int parent_type = state->parent_type;

    for (int i=0; i<size; i++){
        ASSERT_JSMN_TYPE(*tok, JSMN_STRING, j);

        char *key = tok_to_str(*tok, j);

        jsmn_parsing_fn parse = get_parse_fn(key, parent_type);
        if (parse == NULL){
            log_error("No matching key %s for type %d",
                      tok_to_str(*tok, j), parent_type);
            return -1;
        }

        jsmntok_t *pre_tok = *tok;
        void *pre_data = state->data;
        state->tok = ++(*tok);
        int rtn = parse(tok, j, state, saved);

        if (rtn < 0){
            log_error("Error parsing token at %s", &j[pre_tok->start]);
            return -1;
        } else if (rtn > 0){
            log(LOG_JSMN_PARSING, "Deferring parsing of %s", key);
            struct json_state *to_save = malloc(sizeof(*to_save));
            to_save->data = pre_data;
            to_save->parent_type = parent_type;
            to_save->next = *saved;
            to_save->tok = pre_tok;
            *saved = to_save;
        } else {
            log(LOG_JSMN_PARSING, "Successfully parsed %s", key);
        }
        ++(*tok);
    }
    --(*tok);
    return size;
}
