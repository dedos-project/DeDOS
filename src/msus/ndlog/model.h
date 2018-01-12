#ifndef MODEL_H
#define MODEL_H
#include <map>
#include <string>
#include <deque>
#include "value/allValue.h"
#include <openssl/sha.h>

// TODO: You really shouldn't use namespaces in headers!
using namespace std;

enum tuple_type {INSERT, DELETE, RECV, SEND};

typedef struct ndlog_tuple_attribute {
  uint8_t name_len;
  uint32_t data_len;
  string attr_name;
  ND_Value* attr_data; 
} ndlog_tuple_attribute;

typedef struct ndlog_tuple {
  string tuple_name; // should be the same as the table name
  map<string, ndlog_tuple_attribute*> attributes; // attr_name -> attribute
  enum tuple_type tp_Type;
  string data_key; // the SHA1 key of self based on attr_data
} ndlog_tuple;

typedef struct ndlog_state {
    string state_name;
    map<string, ndlog_tuple*> tuples;
    //deque<string> key_attr;
    map<string, string> key_attr; // map-style linked list
} ndlog_state; 

//map<string, ndlog_state*> Database; // now this is not required

#endif
