extern "C" {
#include "local_msu.h"
#include "msu_calls.h"
}
#include "ndlog/ndlog_recv_msu.h"
#include "helper-function.h"
#include <iostream>

using namespace std;

uint32_t readUint32(char* buf)
{
  // return (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | (buf[0]);
  unsigned char *p = (unsigned char *)buf;
  uint32_t r, p0, p1, p2, p3;
  p0 = p[0];
  p1 = p[1];
  p2 = p[2];
  p3 = p[3];
  r = (p0 << 24) + (p1 << 16) + (p2 << 8) + p3;
  return r;
}

/*void AddState(string stateName, string keyList[], int listLen)
{
  ndlog_state* newState = new ndlog_state;
  //ndlog_state* newState = (ndlog_state*) malloc(sizeof(ndlog_state));
  newState->state_name = stateName;
  for (int i = 0; i < listLen; i++) {
  	newState->key_attr[keyList[i]] = keyList[i];
  }
  Database[stateName] = newState;
}*/

ndlog_state* AddState(string stateName, string keyList[], int listLen) 
{
  ndlog_state* newState = new ndlog_state;
  newState->state_name = stateName;
  for (int i = 0; i < listLen; i++) {
    newState->key_attr[keyList[i]] = keyList[i];
  }
  return newState;
}

bool isRecvEvent(ndlog_tuple* arrivalTuple, string stateName) 
{
  return (arrivalTuple->tuple_name == stateName && 
  	arrivalTuple->tp_Type == RECV);
}

/*
ndlog_state* GetState(string stateName) 
{
  if (Database.find(stateName) != Database.end()) {
  	return Database[stateName];
  }
  return NULL;
}*/

ndlog_state* GetState(struct local_msu *self, string stateName)
{
	//printf("Begin GetState\n");
  map<string, ndlog_state*>* internalDB = (map<string, ndlog_state*>*) (self->msu_state);
  //printf("Size of internalDB: %d\n", internalDB->size());
  if (internalDB->find(stateName) != internalDB->end()) {
  	//printf("End GetState: find\n");
    return (*internalDB)[stateName];
  }
  //printf("End GetState: NULL\n");
  return NULL;
}

/*
map<string, ND_Value*> GetAttrData(string stateName, string attrName) 
{
  map<string, ND_Value*> result;
  ndlog_state* tempState = GetState(stateName);
  if (!tempState) {
    log_error("GetAttrData: Table not found");
  }
  else {
    map<string, ndlog_tuple*>::iterator it = tempState->tuples.begin();
    for (; it != tempState->tuples.end(); it++) {
      ndlog_tuple_attribute* tempAttr = get_tuple_attr(it->second, attrName);
      ND_Value* tempValue = tempAttr->attr_data->Clone();
      result[it->first] = tempValue;
    }
  }
  return result;
}*/

map<string, ND_Value*> GetAttrData(struct local_msu *self, string stateName, string attrName)
{
  map<string, ND_Value*> result;
  ndlog_state* tempState = GetState(self, stateName);
  if (!tempState) {
    log_error("GetAttrData: Table not found");
  }
  else {
    map<string, ndlog_tuple*>::iterator it = tempState->tuples.begin();
    for (; it != tempState->tuples.end(); it++) {
      ndlog_tuple_attribute* tempAttr = get_tuple_attr(it->second, attrName);
      ND_Value* tempValue = tempAttr->attr_data->Clone();
      result[it->first] = tempValue;
    }
  }
  return result;
}

deque<ndlog_tuple_attribute*> MakeAttributeList(const string nameList[], int listLen) 
{
  if (listLen == 0) 
  	return deque<ndlog_tuple_attribute*>();
  else {
  	deque<ndlog_tuple_attribute*> result;
  	for (int it = 0; it < listLen; it++) {
      ndlog_tuple_attribute* curr = new ndlog_tuple_attribute;
  	  //ndlog_tuple_attribute* curr = (ndlog_tuple_attribute*) malloc(sizeof(ndlog_tuple_attribute));
      curr->name_len = nameList[it].length() + 1;
      curr->attr_name = nameList[it];
      result.push_back(curr);
  	}
    return result;
  }
}

ndlog_state* do_join_st(ndlog_state* s, ndlog_tuple* t, deque<ndlog_tuple_attribute*> *sa, deque<ndlog_tuple_attribute*> *ta) 
{
	//printf("begin do_join_st\n");
  ndlog_state* result = new ndlog_state;
  //ndlog_state* result = (ndlog_state*) malloc(sizeof(ndlog_state));
  //result->state_name = "JOINEDSTATE";
  for (map<string, string>::iterator it = s->key_attr.begin(); it != s->key_attr.end(); it++) {
  	int step = 0;
  	deque<ndlog_tuple_attribute*>::iterator sa_it = sa->begin();
  	for (; sa_it != sa->end(); sa_it++, step++) {
  		if ((*sa_it)->attr_name == it->second)
  			break;
  	}
  	if (sa_it != sa->end()) {
  		string tempKeyAttr = (*(ta->begin() + step))->attr_name;
  		result->key_attr[tempKeyAttr] = tempKeyAttr;
  	}
  	else {
  		result->key_attr[it->first] = it->second;
  	}
  }

  //printf("finish key_attr setting\n");

  for (map<string, ndlog_tuple*>::iterator it = (s->tuples).begin(); 
    it != (s->tuples).end(); it++) {
  	ndlog_tuple* res = do_join_tt(it->second, t, sa, ta);
    //if (res && result->tuples.find(res->data_key) == result->tuples.end()) {
 		if (res) {
      result->tuples[res->data_key] = res;
 		}
    //}
    /*else if (res) {
      for (map<string, ndlog_tuple_attribute*>::iterator jt = res->attributes.begin(); 
        jt != res->attributes.end(); jt++) {
        delete jt->second->attr_data;
        delete jt->second;
      }
      delete res;
    }*/
  }
  
  /*printf("check join result:\n");
  printf("Size of result table (num of tuples): %d\n", result->tuples.size());
  map<string, ndlog_tuple*>::iterator test_tup_it = result->tuples.begin();
  printf("Size of one tuple (num of attributes): %d\n", test_tup_it->second->attributes.size());
  map<string, ndlog_tuple_attribute*>::iterator test_attr_it = test_tup_it->second->attributes.begin();
  printf("Print attributes:\n");
  for (; test_attr_it != test_tup_it->second->attributes.end(); test_attr_it++) {
  	if (dynamic_cast<ND_Int32Value*> (test_attr_it->second->attr_data) != NULL) {
  		cout << test_attr_it->first << "(" << test_attr_it->second->attr_name << "): int32 "
  		 << (dynamic_cast<ND_Int32Value*> (test_attr_it->second->attr_data))->GetInt32Value() << endl;
  	}
  	else {
  		cout << test_attr_it->first << "(" << test_attr_it->second->attr_name << "): String "
  		 << test_attr_it->second->attr_data->ToString() << endl;
  	}
  }*/
  /*map<string, string>::iterator key_it = result->key_attr.begin();
  cout << "key attr: ";
  for (; key_it != result->key_attr.end(); key_it++) {
  	cout << key_it->second << " ";
  }
  cout << endl;*/
  //printf("check complete\n");

  //printf("do_join_st begin free memory\n");
  for (deque<ndlog_tuple_attribute*>::iterator it = sa->begin(); it != sa->end(); it++) {
    //delete ((*it)->attr_data);
    delete (*it);
  }
  for (deque<ndlog_tuple_attribute*>::iterator it = ta->begin(); it != ta->end(); it++) {
    //delete ((*it)->attr_data);
    delete (*it);
  }
  //printf("End do_join_st\n");
  return result;
} // key attribute the same as that of s

ndlog_tuple* do_join_tt(ndlog_tuple* t1, ndlog_tuple* t2, deque<ndlog_tuple_attribute*> *a1, deque<ndlog_tuple_attribute*> *a2)
{
  deque<ndlog_tuple_attribute*>::iterator at1 = a1->begin() + 1;
  deque<ndlog_tuple_attribute*>::iterator at2 = a2->begin() + 1;
  for (; at1 != a1->end() && at2 != a2->end(); at1++, at2++) {
    //ndlog_tuple_attribute* tmp1 = get_tuple_attr(t1, (*at1)->attr_name);
    //ndlog_tuple_attribute* tmp2 = get_tuple_attr(t2, (*at2)->attr_name);
    ndlog_tuple_attribute* tmp1 = t1->attributes[(*at1)->attr_name];
    ndlog_tuple_attribute* tmp2 = t2->attributes[(*at2)->attr_name];
    if (!tmp1->attr_data->Equals(tmp2->attr_data)) {
    	return NULL;
    }
  }

  ndlog_tuple* result = new ndlog_tuple;
  //ndlog_tuple* result = (ndlog_tuple*) malloc(sizeof(ndlog_tuple));
  result->data_key = t1->data_key;
  //result->tuple_name = "JOINEDSTATE";

  map<string, ndlog_tuple_attribute*>::iterator t1attr = t1->attributes.begin();

  while (t1attr != t1->attributes.end())
  {
    if (!has_attr(a1, t1attr->second->attr_name))
      do_append(result, t1attr->second);
    t1attr++;
  }

  map<string, ndlog_tuple_attribute*>::iterator t2attr = t2->attributes.begin();

  while (t2attr != t2->attributes.end())
  {
    do_append(result, t2attr->second);
    t2attr++;
  }
  return result;
} // joined attr name is given by a2 not a1. key data is given only by t1

ndlog_tuple_attribute* get_tuple_attr(ndlog_tuple* t, string name)
{
  map<string, ndlog_tuple_attribute*>::iterator it = t->attributes.begin();
  for (; it != t->attributes.end(); it++) {
  	if (it->second->attr_name == name) {
  	  return it->second;
  	}
  }
  return NULL;
}

bool has_attr(deque<ndlog_tuple_attribute*> *a, string name)
{
  deque<ndlog_tuple_attribute*>::iterator it = a->begin();
  for (; it != a->end(); it++) {
  	if ((*it)->attr_name == name) 
  	  return true;
  }
  return false;
}

void do_append(ndlog_tuple* tup, ndlog_tuple_attribute* attr)
{
  ndlog_tuple_attribute* a = new ndlog_tuple_attribute;
  //ndlog_tuple_attribute* a = (ndlog_tuple_attribute*) malloc(sizeof(ndlog_tuple_attribute));
  copy_attr(a, attr);
  tup->attributes[a->attr_name] = a;
}

void copy_attr(ndlog_tuple_attribute* dst, ndlog_tuple_attribute* src)
{
  dst->name_len = src->name_len;
  dst->attr_name = src->attr_name;
  dst->data_len = src->data_len;
  dst->attr_data = src->attr_data->Clone();
}

/*
ndlog_state* do_project_s(ndlog_state* s, deque<ndlog_tuple_attribute*> *old_attrs, deque<ndlog_tuple_attribute*> *new_attrs, string stateName) 
{
  ndlog_state* result = new ndlog_state;
  //ndlog_state* result = (ndlog_state*) malloc(sizeof(ndlog_state));
  result->state_name = stateName;
  if (Database->find(stateName) != Database->end()) {
  	result->key_attr = Database[stateName]->key_attr;
  }
  else {
  	for (deque<ndlog_tuple_attribute*>::iterator it = old_attrs->begin(); it != old_attrs->end(); it++) {
  	  result->key_attr[(*it)->attr_name] = (*it)->attr_name; 
  	}
  }

  map<string, ndlog_tuple*>::iterator it = s->tuples.begin();

  for (; it != s->tuples.end(); it++) {
  	ndlog_tuple* newtup = do_project_t(it->second, old_attrs, new_attrs, &(result->key_attr)); // should finish attr_name/data_key change
  	newtup->tuple_name = stateName;
  	if (result->tuples.find(newtup->data_key) == result->tuples.end()) {
  	  result->tuples[newtup->data_key] = newtup;
  	}
  }
  return result;
}*/

ndlog_state* do_project_s(struct local_msu *self, ndlog_state* s, deque<ndlog_tuple_attribute*> *old_attrs, deque<ndlog_tuple_attribute*> *new_attrs, string stateName) 
{
	//printf("begin do_project_s\n");
  ndlog_state* result = new ndlog_state;
  //ndlog_state* result = (ndlog_state*) malloc(sizeof(ndlog_state));
  result->state_name = stateName;
  map<string, ndlog_state*>* internalDB = (map<string, ndlog_state*>*) (self->msu_state);
  if (internalDB->find(stateName) != internalDB->end()) {
    result->key_attr = ((*internalDB)[stateName])->key_attr;
  }
  else {
    for (deque<ndlog_tuple_attribute*>::iterator it = old_attrs->begin(), jt = new_attrs->begin(); 
      it != old_attrs->end() && jt != new_attrs->end(); it++, jt++) {
      if (s->key_attr.find((*it)->attr_name) != s->key_attr.end()) {
        result->key_attr[(*jt)->attr_name] = (*jt)->attr_name; 
      }
    }
  } // TODO: key setting still unsure

  map<string, ndlog_tuple*>::iterator it = s->tuples.begin();

  for (; it != s->tuples.end(); it++) {
    ndlog_tuple* newtup = do_project_t(it->second, old_attrs, new_attrs, &(result->key_attr)); // should finish attr_name/data_key change
    newtup->tuple_name = stateName;
    if (result->tuples.find(newtup->data_key) == result->tuples.end()) {
      result->tuples[newtup->data_key] = newtup;
    }
    else {
      for (map<string, ndlog_tuple_attribute*>::iterator jt = newtup->attributes.begin(); 
        jt != newtup->attributes.end(); jt++) {
        delete jt->second->attr_data;
        delete jt->second;
      }
      delete newtup;
    }
  }

  for (deque<ndlog_tuple_attribute*>::iterator it = old_attrs->begin(); it != old_attrs->end(); it++) {
    //delete (*it)->attr_data;
    delete (*it);
  }
  for (deque<ndlog_tuple_attribute*>::iterator it = new_attrs->begin(); it != new_attrs->end(); it++) {
    //delete (*it)->attr_data;
    delete (*it);
  }

  /*printf("check project result:\n");
  printf("result_state name: %s\n", result->state_name.c_str());
  cout << "check key attr: ";
  for (map<string, string>::iterator key_it = result->key_attr.begin(); key_it != result->key_attr.end(); key_it++) {
  	cout << key_it->first << " ";
  }
  cout << endl;
  printf("Size of table (num of tuple): %d\n", result->tuples.size());
  map<string, ndlog_tuple*>::iterator tuple_it = result->tuples.begin();
  printf("Tuple name: %s\n", tuple_it->second->tuple_name.c_str());
  printf("Tuple attributes: \n");
  for (map<string, ndlog_tuple_attribute*>::iterator attr_it = tuple_it->second->attributes.begin(); 
  	attr_it != tuple_it->second->attributes.end(); attr_it++) {
  	cout << attr_it->first << "(" << attr_it->second->attr_name << "): " << attr_it->second->attr_data->ToString() << endl;
  }
  printf("check complete\n");*/
  //printf("end do_project_s\n");
  return result;
}

ndlog_tuple* do_project_t(ndlog_tuple* t, deque<ndlog_tuple_attribute*> *old_attrs, deque<ndlog_tuple_attribute*> *new_attrs, map<string, string> *key_attr) 
{
  ndlog_tuple* result = new ndlog_tuple;
  //ndlog_tuple* result = (ndlog_tuple*) malloc(sizeof(ndlog_tuple));
  deque<ndlog_tuple_attribute*>::iterator old_it = old_attrs->begin();
  deque<ndlog_tuple_attribute*>::iterator new_it = new_attrs->begin();
  string keyBase = "";

  for (; old_it != old_attrs->end() && new_it != new_attrs->end(); old_it++, new_it++) {
    ndlog_tuple_attribute* tempAttr = new ndlog_tuple_attribute;
  	//ndlog_tuple_attribute* tempAttr = (ndlog_tuple_attribute*) malloc(sizeof(ndlog_tuple_attribute));
  	copy_attr(tempAttr, t->attributes[(*old_it)->attr_name]);
  	tempAttr->name_len = (*new_it)->name_len;
  	tempAttr->attr_name = (*new_it)->attr_name;
  	if (key_attr->find(tempAttr->attr_name) != key_attr->end()) {
  	  keyBase += tempAttr->attr_data->ToString();
  	}
  	result->attributes[(*new_it)->attr_name] = tempAttr;
  }

  unsigned char obuf[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(keyBase.c_str()), keyBase.length (), obuf);
  string shaKey(reinterpret_cast<char*>(obuf));
  result->data_key = shaKey;

  return result;
} // result's tuple_name and type need to be modified.

void updateTupleType(ndlog_state* state, enum tuple_type tpType) {
  map<string, ndlog_tuple*>::iterator it = state->tuples.begin();
  for(; it != state->tuples.end(); it++) {
    it->second->tp_Type = tpType;
  }
}

void copy_tuple(ndlog_tuple* dst, ndlog_tuple* src) 
{
  dst->tuple_name = src->tuple_name;
  dst->data_key = src->data_key;
  map<string, ndlog_tuple_attribute*>::iterator it = src->attributes.begin();
  for (; it != src->attributes.end(); it++) {
    ndlog_tuple_attribute* tempAttr = new ndlog_tuple_attribute;
    copy_attr(tempAttr, it->second);
    dst->attributes[it->first] = tempAttr;
  }
} // tuple_name copied

void result_to_msu_calls(ndlog_state* result, struct local_msu *sender, struct msu_msg_hdr *hdr)
{
  map<string, ndlog_tuple*>::iterator res_it = result->tuples.begin();
  for (; res_it != result->tuples.end(); res_it++) {
    call_msu_type(sender, &NDLOG_RECV_MSU_TYPE, hdr, sizeof(res_it->second), (void*)res_it->second);
  }
} // TODO: buffer length, free buffer?

// The following functions are for packet forwarding test
uint32_t
AsciiToIpv4Host (char const *address)
{
  uint32_t host = 0;
  while (true) {
    uint8_t byte = 0;
    while (*address != ASCII_DOT &&
           *address != 0) {
      byte *= 10;
      byte += *address - ASCII_ZERO;
      address++;
    }
    host <<= 8;
    host |= byte;
    if (*address == 0) {
      break;
    }
    address++;
  }
  return host;
}

void AddData(map<string, ndlog_state*>* DB, string tableName)
{
  ndlog_state* table = (*DB)[tableName]; // TODO: log_error
  const char* ipS = "192.168.13.37";
  uint32_t ipServer, ipSrc, ipDst;
  ipServer = AsciiToIpv4Host(ipS);
  //int i = 0;
  srand(1);
  for (int i = 0; i < LOOKUPTABLESIZE; i++) {
    //ipSrc = (uint32_t) rand();
    //ipDst = (uint32_t) rand();
    if (i == 0) {
    	ipSrc = AsciiToIpv4Host("192.168.13.13");
    	ipDst = 20;
    }
    else if (i == 1) {
    	ipSrc = AsciiToIpv4Host("192.168.13.14");
    	ipDst = 20; 
    }
    else if (i == 3) {
    	ipSrc = AsciiToIpv4Host("192.168.13.14");
    	ipDst = 20;
    }
    else {
    	ipSrc = (uint32_t) rand();
      ipDst = (uint32_t) rand();
    }

    ndlog_tuple* newtup = new ndlog_tuple;
    newtup->tuple_name = tableName;

    ndlog_tuple_attribute* server_attr = new ndlog_tuple_attribute;
    ndlog_tuple_attribute* src_attr = new ndlog_tuple_attribute;
    ndlog_tuple_attribute* dst_attr = new ndlog_tuple_attribute;

    server_attr->attr_name = "lookup_attr1";
    server_attr->name_len = server_attr->attr_name.length() + 1;
    server_attr->attr_data = new ND_Int32Value (ipServer);
    server_attr->data_len = sizeof(ipServer);

    //cout << "ipServer: " << ipServer << endl;
    //cout << (dynamic_cast<ND_Int32Value*> (server_attr->attr_data))->GetInt32Value() << endl;

    src_attr->attr_name = "lookup_attr2";
    src_attr->name_len = src_attr->attr_name.length() + 1;
    src_attr->attr_data = new ND_Int32Value (ipSrc);
    src_attr->data_len = sizeof(ipSrc);

    dst_attr->attr_name = "lookup_attr3";
    dst_attr->name_len = dst_attr->attr_name.length();
    dst_attr->attr_data = new ND_Int32Value (ipDst);
    dst_attr->data_len = sizeof(ipDst);

    string keyBase = src_attr->attr_data->ToString();
    unsigned char obuf[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(keyBase.c_str()), keyBase.length (), obuf);
    string shaKey(reinterpret_cast<char*>(obuf));
    newtup->data_key = shaKey;

    newtup->attributes["lookup_attr1"] = server_attr;
    newtup->attributes["lookup_attr2"] = src_attr;
    newtup->attributes["lookup_attr3"] = dst_attr;

    if (table->tuples.find(newtup->data_key) == table->tuples.end()) {
    	//printf("Add success\n");
      table->tuples[newtup->data_key] = newtup;
    }
    else {
    	for (map<string, ndlog_tuple_attribute*>::iterator jt = newtup->attributes.begin(); 
        jt != newtup->attributes.end(); jt++) {
        delete jt->second->attr_data;
        delete jt->second;
      }
      delete newtup;
    }
  } 
}
