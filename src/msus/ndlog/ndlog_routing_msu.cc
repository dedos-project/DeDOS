extern "C" {
#include "msu_message.h"
#include "msu_calls.h"
}
#include "ndlog/ndlog_routing_msu.h"
#include "ndlog/ndlog_msu_R1_eca.h"
#include "logging.h"
#include "ndlog/helper-function.h"
#include <stdio.h>
#include <iostream>

using namespace std;

struct local_msu* ndlog_routing_msu;
uint32_t count_packets;

/*uint32_t readUint32(char* buf)
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
}*/

int ndlog_routing_msu_receive(struct local_msu *self, struct msu_msg *msg)
{
  // TODO: queue_item be buffer of char*, change it to tuple 
  //printf("ndlog_routing_msu: Receive data\n");
  char* p = (char*) msg->data;
  uint32_t mdst = readUint32(p);
  p += 4;
  uint32_t msrc = readUint32(p);
  p += 4;
  uint16_t dlen = (uint16_t) msg->data_size - 8;
  char* mdata = (char*) malloc(dlen);
  memcpy(mdata, p, dlen);

  //cout << mdst << " " << msrc << endl;;

  ndlog_tuple* newtp = new ndlog_tuple;
  newtp->tuple_name = "packetIn";
  newtp->data_key = "";

  ndlog_tuple_attribute* server_attr = new ndlog_tuple_attribute;
  server_attr->attr_name = "packetIn_attr1";
  server_attr->name_len = server_attr->attr_name.length() + 1;
  server_attr->data_len = sizeof(uint32_t);
  server_attr->attr_data = new ND_Int32Value (mdst);

  ndlog_tuple_attribute* src_attr = new ndlog_tuple_attribute;
  src_attr->attr_name = "packetIn_attr2";
  src_attr->name_len = src_attr->attr_name.length() + 1;
  src_attr->data_len = sizeof(uint32_t);
  src_attr->attr_data = new ND_Int32Value (msrc);

  ndlog_tuple_attribute* DATA_attr = new ndlog_tuple_attribute;
  DATA_attr->attr_name = "packetIn_attr3";
  DATA_attr->name_len = DATA_attr->attr_name.length() + 1;
  DATA_attr->data_len = dlen;
  string tempData(mdata); // TODO: free mdata
  DATA_attr->attr_data = new ND_StrValue (tempData);

  newtp->attributes["packetIn_attr1"] = server_attr;
  newtp->attributes["packetIn_attr2"] = src_attr;
  newtp->attributes["packetIn_attr3"] = DATA_attr;

  struct msu_msg_key key;
  set_msg_key((int32_t)count_packets++, &key);
  //printf("ndlog_routing_msu: receive end\n");
  return init_call_msu_type(self, &NDLOG_MSU_TYPE, &key, sizeof(newtp), (void*) newtp);
}

int ndlog_routing_msu_init(struct local_msu *self, struct msu_init_data *init_data)
{
  ndlog_routing_msu = self;
  count_packets = 0;
  log_critical("Created %s MSU with id: %u", self->type->name, self->id);
  return 0;
}


struct msu_type NDLOG_ROUTING_MSU_TYPE = {
    .name=(char*)"ndlog_routing_msu",
    .id=NDLOG_ROUTING_MSU_ID, // TODO: need to define it in runtime/src/include/dedos_msu_list.h
    .init_type=NULL,
    .destroy_type=NULL,
    .init=ndlog_routing_msu_init,
    .destroy=NULL,
    .receive=ndlog_routing_msu_receive,
    .route=NULL,
    .serialize=NULL,
    .deserialize=NULL
};
