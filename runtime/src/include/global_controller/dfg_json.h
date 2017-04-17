#ifndef DFG_JSON_H_
#define DFG_JSON_H_

#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "jsmn.h"
#include "dfg.h"

/* Some JSON properties */
#define MAX_JSON_LEN 65536
#define MAX_MSU 128
// Counts are inclusive of ALL subfields
#define RUNTIME_FIELDS 6
#define MSU_PROFILING_FIELDS 10

/* Some struct related to DFG management protocol */

int do_dfg_config(const char *init_filename);
void print_dfg();
//void dump_json(void);
int jsoneq(const char *json, jsmntok_t *tok, const char *s);
void json_parse_error(int err);
int json_test(jsmntok_t *t, int r);
struct dfg_vertex; //for next signatures to refer to this 
void parse_msu_scheduling(jsmntok_t *t, int *starting_token,
                          const char *json_string, struct dfg_vertex *v);
int parse_msu_profiling(jsmntok_t *t, int starting_token,
                        const char *json_string, struct dfg_vertex *v);
int json_parse_runtimes(jsmntok_t *t, int starting_token,
                        int ending_token, const char *json_string);
int json_parse_msu(jsmntok_t *t, int starting_token, int msu_cnt, const char *json_string);

#endif //DFG_JSON_H_
