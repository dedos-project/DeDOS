#ifndef HELPERFUNCTION_H
#define HELPERFUNCTION_H

#include "model.h"
#include "local_msu.h"
#include <stdlib.h>
#include <map>
#include <deque>
#include <openssl/sha.h>
#include <stdint.h>

using namespace std;

uint32_t readUint32(char* buf);

// void AddState(string stateName, string keyList[] = NULL, int listLen = 0);

// tuples are not set yet
ndlog_state* AddState(string stateName, string keyList[] = NULL, int listLen = 0);

bool isRecvEvent(ndlog_tuple* arrivalTuple, string stateName);

// ndlog_state* GetState(string stateName); 

ndlog_state* GetState(struct local_msu *self, string stateName); 

// map<string, ND_Value*> GetAttrData(string stateName, string attrName);

map<string, ND_Value*> GetAttrData(struct local_msu *self, string stateName, string attrName);

// only set attr_name and name_len, return value can not be used to do data transfering
deque<ndlog_tuple_attribute*> MakeAttributeList(const string nameList[], int listLen);

// state_name is JOINEDSTATE
// return state is dynamically allocated
ndlog_state* do_join_st(ndlog_state* s, ndlog_tuple* t, deque<ndlog_tuple_attribute*> *sa, deque<ndlog_tuple_attribute*> *ta);

// tuple_name is JOINEDSTATE
ndlog_tuple* do_join_tt(ndlog_tuple* t1, ndlog_tuple* t2, deque<ndlog_tuple_attribute*> *a1, deque<ndlog_tuple_attribute*> *a2);

ndlog_tuple_attribute* get_tuple_attr(ndlog_tuple* t, string name);

bool has_attr(deque<ndlog_tuple_attribute*> *a, string name);

void do_append(ndlog_tuple* tup, ndlog_tuple_attribute* attr);

void copy_attr(ndlog_tuple_attribute* dst, ndlog_tuple_attribute* src);

// ndlog_state* do_project_s(ndlog_state* s, deque<ndlog_tuple_attribute*> *old_attrs, deque<ndlog_tuple_attribute*> *new_attrs, string stateName);

ndlog_state* do_project_s(struct local_msu *self, ndlog_state* s, deque<ndlog_tuple_attribute*> *old_attrs, deque<ndlog_tuple_attribute*> *new_attrs, string stateName);

ndlog_tuple* do_project_t(ndlog_tuple* t, deque<ndlog_tuple_attribute*> *old_attrs, deque<ndlog_tuple_attribute*> *new_attrs, map<string, string> *key_attr);

void copy_tuple(ndlog_tuple* dst, ndlog_tuple* src);

void updateTupleType(ndlog_state* state, enum tuple_type tpType);

void result_to_msu_calls(ndlog_state* result, struct local_msu *sender, struct msu_msg_hdr *hdr);

// the following functions are for packet forwarding test use
#define LOOKUPTABLESIZE 1000
#define ASCII_DOT (0x2e)
#define ASCII_ZERO (0x30)

uint32_t AsciiToIpv4Host (char const *address);

void AddData(map<string, ndlog_state*>* DB, string tableName);

#endif
