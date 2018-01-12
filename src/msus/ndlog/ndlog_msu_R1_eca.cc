extern "C" {
#include "ndlog/ndlog_msu_R1_eca.h"
#include "logging.h"
#include <stdio.h>
#include "local_msu.h"
#include "msu_message.h"
}
#include <map>
#include <string>
#include "model.h"
#include "helper-function.h"

using namespace std;
/*
void InitDataBase() 
{
  string lookup_keyList[] = {"lookup_attr2"};
  AddState("lookup", lookup_keyList, 1);
  AddData("lookup"); // manually insert data to the specific table
}*/

int ndlog_msu_R1_eca_init(struct local_msu *self, struct msu_init_data *init_data)
{
  map<string, ndlog_state*> initialDB;

  string lookup_keyList[] = {"lookup_attr2"};
  //initialDB["lookup"] = AddState("lookup", lookup_keyList, 1);
  //AddData(&initialDB, "lookup"); // manually insert data to the specific table
  self->msu_state = new map<string, ndlog_state*>;
  (*((map<string, ndlog_state*>*) (self->msu_state)))["lookup"] = AddState("lookup", lookup_keyList, 1);
  //printf("finish addstate\n");
  AddData((map<string, ndlog_state*>*) (self->msu_state), "lookup");

  //self->msu_state = (void *)(&initialDB);
  //log_critical("initialDB size: %d\n", initialDB.size());
  //printf("Size of msu_state: %d\n", ((*((map<string, ndlog_state*>*) (self->msu_state)))["lookup"])->tuples.size());
  log_critical("Created %s MSU with id: %u", self->type->name, self->id);
  return 0;
}

/*
void R1_eca(ndlog_tuple* packetin) 
{
  string joinLookupAttributes[] = {"lookup_attr1", "lookup_attr2"};
  string joinPacketinAttributes[] = {"packetIn_attr1", "packetIn_attr2"};
  ndlog_state* joinResult = do_join_st(GetState("lookup"), 
  	packetin, 
  	&MakeAttributeList(joinLookupAttributes, 2), 
  	&MakeAttributeList(joinPacketinAttributes, 2));

  string projectAttributes[] = {"lookup_attr3", "packetIn_attr2", "packetIn_attr3"};
  string projectResultAttributes[] = {"packetOut_attr1", "packetOut_attr2", "packetOut_attr3"};
  ndlog_state* projectResult = do_project_s(joinResult,
  	MakeAttributeList(projectAttributes, 3), 
  	MakeAttributeList(projectResultAttributes, 3), 
  	"packetOut");
  
  enum tuple_type updateType = SEND;
  updateTupleType(projectResult, updateType);
  // TODO: send
  //int default_send_remote(struct generic_msu *src, msu_queue_item *data,
  //                      struct msu_endpoint *dst)
} // need to free memory: projectResult, joinResult
*/

int ndlog_msu_R1_eca_receive(struct local_msu *self, struct msu_msg *msg) 
{
  //printf("Size of msu_state: %d\n", ((map<string, ndlog_state*>*) (self->msu_state))->size());
  //printf("ndlog_msu_R1_eca: Receive data\n");
  ndlog_tuple* packetin = (ndlog_tuple*) (msg->data);
  
  string joinLookupAttributes[] = {"lookup_attr1", "lookup_attr2"};
  string joinPacketinAttributes[] = {"packetIn_attr1", "packetIn_attr2"};
  deque<ndlog_tuple_attribute*> joinLookupAttributesList = MakeAttributeList(joinLookupAttributes, 2);
  deque<ndlog_tuple_attribute*> joinPacketinAttributesList = MakeAttributeList(joinPacketinAttributes, 2);
  //printf("MakeAttributeList finished\n");
  ndlog_state* joinResult = do_join_st(GetState(self, "lookup"), 
    packetin, 
    &joinLookupAttributesList, 
    &joinPacketinAttributesList);

  
  string projectAttributes[] = {"lookup_attr3", "packetIn_attr2", "packetIn_attr3"};
  string projectResultAttributes[] = {"packetOut_attr1", "packetOut_attr2", "packetOut_attr3"};
  deque<ndlog_tuple_attribute*> projectAttributesList = MakeAttributeList(projectAttributes, 3);
  deque<ndlog_tuple_attribute*> projectResultAttributesList = MakeAttributeList(projectResultAttributes, 3);
  ndlog_state* projectResult = do_project_s(self, joinResult,
    &projectAttributesList, 
    &projectResultAttributesList, 
    "packetOut");
  //printf("end project\n");

  //enum tuple_type updateType = SEND;
  //updateTupleType(projectResult, updateType);
  //printf("%d\n", projectResult->tuples.size());
  result_to_msu_calls(projectResult, self, &msg->hdr);

  return 0;
  //printf("ndlog_msu_R1_eca: Receive finished\n");

  //return call_msu_type(self, NDLOG_RECV_MSU_TYPE, msg->hdr, ...);
  //return DEDOS_NDLOG_RECV_MSU_ID;
}

/*void DemuxRecv(ndlog_tuple* arrivalTuple) 
{
  if (isRecvEvent(arrivalTuple, "packetIn")) {
    ndlog_msu_R1_eca_receive(arrivalTuple);
  }
}*/ // TODO: should be included later

extern "C" {
struct msu_type NDLOG_MSU_TYPE = {
    .name=(char*)"ndlog_msu",
    .id=NDLOG_MSU_TYPE_ID,
    .init_type=NULL,
    .destroy_type=NULL,
    .init=ndlog_msu_R1_eca_init,
    .destroy=NULL,
    .receive=ndlog_msu_R1_eca_receive,
    .route=NULL,
    .serialize=NULL,
    .deserialize=NULL
};
}
