/**
 * @file jsmn_parser.h
 *
 * General-purpose function to interact with JSMN library, and create
 * objects (potentially with circular references) from the parsed
 * json.
 */

#ifndef JSMN_PARSER_H_
#define JSMN_PARSER_H_
#include "jsmn.h"

#include <stdlib.h>
#include <stdio.h>

/**
 * Asserts that the type of the current token is the provided type,
 * and logs an error if that is not the case
 */
#define ASSERT_JSMN_TYPE(tok, toktype, j) \
    if ((tok)->type != toktype){ \
        log_error("Got type %d, expected %d while parsing %.50s", \
                  (tok)->type, toktype, &j[(tok)->start]); \
        return -1; \
    } \

/**
 * Structure to hold state while parsing JSON
 */
struct json_state {
    void *data;       /**< The object currently under construction */
    int parent_type;  /**< The type of object that is being create */
    struct json_state *next; /** When saving state, provides a linked-list */
    jsmntok_t *tok;   /**< The json token currently being read */
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
 * Macro for defining a jsmn_parsing_fn.
 * Each key in the JSON file should have an associated function defined using
 * this macro.
 * @param fn_name The name of the function to define, which should return
 *                0 on success, -1 on error, 1 to defer (see ::jsmn_parsing_fn)
 */
#define PARSE_FN(fn_name) \
    static int fn_name(jsmntok_t **tok__, char *j__, \
                     struct json_state *in__, struct json_state **saved__)

/** Within a #PARSE_FN, gets the object currently being constructed */
#define GET_PARSE_OBJ() \
    (in__->data)

/** Within a #PARSE_FN, gets the token being read as a string */
#define GET_STR_TOK() \
    tok_to_str(*tok__, j__)

/** Within a #PARSE_FN, gets the token being read as an integer */
#define GET_INT_TOK() \
    tok_to_int(*tok__, j__)

/** Within a #PARSE_FN, gets the token being read as a long */
#define GET_LONG_TOK() \
    tok_to_long(*tok__, j__)

/**
 * Macro for parsing a list of objects in a json file.
 * For each element that is encountered, `init_fn` will be called to construct
 * the object which is passed in to each of the keys within that object
 * @param fn_name A name for the function to define
 * @param init_fn An #INIT_OBJ_FN to create the new object
 */
#define PARSE_OBJ_LIST_FN(fn_name, init_fn) \
    static int fn_name(jsmntok_t **tok__, char *j__, \
                       struct json_state *in__, struct json_state **saved__) { \
        return parse_jsmn_obj_list(tok__, j__, in__, saved__, init_fn); \
    }

/**
 * Macro for instantiating a new struct based on the appearance of a new JSON object
 * @param fn_name Name for the function, which should subsequently be defined to
 *                create a new object, which should be returned with #RETURN_OBJ
 */
#define INIT_OBJ_FN(fn_name) \
    static struct json_state fn_name(struct json_state *in__, int index__)

/**
 * In an #INIT_OBJ_FN, returns the index of the currently parsed object list
 */
#define GET_OBJ_INDEX() index__

/**
 * Should be the last line in an #INIT_OBJ_FN.
 * Returns the newly-created object.
 * @param data__ A pointer to the newly-created object which should be parsed into
 * @param type__ The enumerator for the type of object which is new being parsed
 */
#define RETURN_OBJ(data__, type__) \
    struct json_state out_obj__ = { \
        .data = data__, \
        .parent_type = type__ \
    }; \
    return out_obj__;

/**
 * Macro to descend into a JSON object and the corresponding C struct for parsing
 * @param fn_name The name of the function to later reference in the key_mapping
 * @param parent_obj_type The typename of the parent C struct already being parsed
 * @param parent_obj_field The field name of the parent C struct which should be
 *                         descended into
 * @param new_type An identifier (enumerator) for the new type of object being created
 */
#define PARSE_OBJ_FN(fn_name, parent_obj_type, parent_obj_field, new_type) \
    static int fn_name(jsmntok_t **tok__, char *j__, \
                       struct json_state *in__, struct json_state **saved__) { \
        parent_obj_type *type_out__ = in__->data; \
        struct json_state state_out__ = { \
            .data = &type_out__->parent_obj_field, \
            .parent_type = new_type, \
            .tok = *tok__ \
        }; \
        if (parse_jsmn_obj(tok__, j__, &state_out__, saved__) < 0) { \
            return -1; \
        } \
        return 0;  \
    }

/**
 * Macro to iterate over a list of tokens.
 * Once inside the iteration, can get the current token with GET_*_TOK().
 * Iteration should be ended with #END_ITER_TOK_LIST
 * @param i An integer into which to place the current index
 */
#define START_ITER_TOK_LIST(i)  \
    int tok_size__ = (*tok__)->size; \
    for (i = 0, ++(*tok__); i< tok_size__;  ++(*tok__), i++)

/**
 * Macro that should appear at the end of the iteration of a list of tokens
 * @param i The same integer which was passed into #START_ITER_TOK_LIST
 */
#define END_ITER_TOK_LIST(i) \
    *tok__ = (*tok__) - (tok_size__ - i + 1);

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
 * Can iterate multiple times on tokens which have dependencies.
 *
 * @param filename - JSON file to be parsed
 * @param obj - object to be filled with the results of parsing
 * @param keymap - the mapping between the JSON key and the function to be called
 * @return 0 on success, -1 on error
 */
int parse_file_into_obj(const char *filename, void *obj, struct key_mapping *km);

/**
 * Using the provided functions, parses the JSON present in
 * in the provided string and stores the resulting object in 'obj'.
 * Can iterate multiple times on tokens which have dependencies.
 *
 * @param contents - Null-terminated string to be parsed
 * @param obj - object to be filled with the results of parsing
 * @param keymap - the mapping between the JSON key and the function to be called
 * @return 0 on success, -1 on error
 */
int parse_str_into_obj(char *contents, void *obj, struct key_mapping *km);


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


/** SHOULD NOT BE USED DIRECTLY. */
int parse_jsmn_obj(jsmntok_t **tok, char *j, struct json_state *in,
                          struct json_state **saved);

/** SHOULD NOT BE USED DIRECTLY.  */
int parse_jsmn_obj_list(jsmntok_t **tok, char *j, struct json_state *in,
                               struct json_state **saved, jsmn_initializer init);

/** Returns the initial object passed into the JSON parser */
void *get_root_jsmn_obj();

/** SHOULD NOT BE USED DIRECTLT */
char *tok_to_str(jsmntok_t *tok, char *j);

/** SHOULD NOT BE USED DIRECTLY */
int tok_to_int(jsmntok_t *tok, char *j);

/** SHOULD NOT BE USED DIRECTLY */
long tok_to_long(jsmntok_t *tok, char *j);

#endif
