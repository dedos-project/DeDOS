#include "ndlog/ndlog_recv_msu.h"
#include "msu_message.h"
#include "logging.h"
#include "local_msu.h"
#include "ndlog/timing.h"
#include <time.h>
#include <inttypes.h>
#include <sys/time.h>
#include <stdio.h>
#include "model.h"
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <time.h>
#include <vector>

using namespace std;

recv_state* recv_msu_global_state;
int checkPkt[9] = {1000, 5000, 10000, 15000, 20000, 25000, 30000, 35000, 40000};
bool first_time = true;

#ifndef SECOND_DATA_FLOW
int line_pos[9] = {139, 140, 141, 142, 143, 144, 145, 146, 147};
#else 
int line_pos[9] = {172, 173, 174, 175, 176, 177, 178, 179, 180}; // for the second data flow
#endif

vector<string> file_vec;

int ndlog_recv_msu_receive(struct local_msu *self, struct msu_msg *msg)
{
  //printf("Begin ndlog_recv_msu_receive:\n");
  //time_t timer;
  //char buffer[26];
  //struct tm* tm_info;

  log_debug("RECV msu %d dequeued item", self->id);
  // fprintf(stderr, "\t\t\t\t\t\t\t\t\t\t\t(%d) received item\n", msu->id);
  //recv_state* s = (recv_state*)(self->internal_state);
  if (recv_msu_global_state->first_packet == 1) // first packet
  {
    clock_gettime(CLOCK_REALTIME, &(recv_msu_global_state->ts));
    recv_msu_global_state->first_packet = 0;
    recv_msu_global_state->received += msg->data_size;
    recv_msu_global_state->count += 1;
    /*FILE* pFile;
    pFile = fopen("/home/jingyuq/mySummerResearch/Dedos_tmp/nd_client/test_time_recv.txt", "a");
    fprintf(pFile, "%lld.%.9ld\n", (long long)recv_msu_global_state->ts.tv_sec, recv_msu_global_state->ts.tv_nsec);
    fclose(pFile);*/
    //clock_gettime(CLOCK_REALTIME, &(s->ts));
    //s->first_packet = 0;
  }
  else
  {
    clock_gettime(CLOCK_REALTIME, &(recv_msu_global_state->te));
    recv_msu_global_state->received += msg->data_size;
    recv_msu_global_state->count += 1;
    /*if (recv_msu_global_state->count % 10 == 0) {
    	uint64_t diff = BILLION * (recv_msu_global_state->te.tv_sec - recv_msu_global_state->ts.tv_sec) + recv_msu_global_state->te.tv_nsec - recv_msu_global_state->ts.tv_nsec;
        double throughput = recv_msu_global_state->count * BILLION * 1.0f / diff;
	    FILE* pFile;
	    pFile = fopen("/home/jingyuq/mySummerResearch/Dedos_tmp/timeseries_throughput.txt", "a");
	    fprintf(pFile, "%lf\n", throughput);
	    fclose(pFile);
	  uint64_t diff = BILLION * (recv_msu_global_state->te.tv_sec - recv_msu_global_state->ts.tv_sec) + recv_msu_global_state->te.tv_nsec - recv_msu_global_state->ts.tv_nsec;
      double throughput = recv_msu_global_state->count * BILLION * 1.0f / diff;
	  FILE* pFile; 
      pFile = fopen("/home/jingyuq/mySummerResearch/Dedos_tmp/out.json", "w");
      string dt = "            \"";
      stringstream ss;
      ss << throughput;
      dt += ss.str();
      ss.str("");
      dt += "\"";
      //dt += ",";
      dt += "\n";

      file_vec[line_pos[0] + 7].replace(file_vec[line_pos[0] + 7].end()-1, file_vec[line_pos[0] + 7].end(), ",\n");
      for (int j = 0; j < 8; j++) {
      	file_vec[line_pos[0] + j - 1] = file_vec[line_pos[0] + j];
      }
      file_vec[line_pos[0] + 7] = dt;
      for (int i = 0; i < file_vec.size(); i++) {
      	fprintf(pFile, file_vec[i].c_str());
      }
      fclose(pFile);
	}

  	if (recv_msu_global_state->count == 5000 && first_time) {
  		recv_msu_global_state->count = 0;
  		recv_msu_global_state->received = 0;
  		recv_msu_global_state->first_packet = 1;
  		first_time = false;
  	}*/
  }

  log_debug("%s", "Dequeued item, freed memory");
  // fprintf(stderr, "%lu\n", (unsigned long)time(NULL));
  //printf("End ndlog_recv_msu_receive\n");
  return -1;
}

int ndlog_recv_msu_init(struct local_msu *self, struct msu_init_data *init_data)
{
  recv_msu_global_state = new recv_state;
  recv_msu_global_state->count = 0;
  recv_msu_global_state->received = 0;
  recv_msu_global_state->first_packet = 1;
  //recv_msu_global_state->has_showed = false;
  recv_msu_global_state->experiment_num = 0;

  FILE* pFile; 
  char jk[100];

#ifndef SECOND_DATA_FLOW
  //pFile = fopen("/home/jingyuq/mySummerResearch/Dedos_tmp/input_cl.json", "r");
  pFile = fopen("/home/jingyuq/mySummerResearch/Dedos_tmp/test_copy.json", "r");
  for (int line = 0; line < 152; line++) {
    char *rtn = fgets(jk, 100, pFile);
    if (rtn == NULL) {
        log_perror("Error in fgets");
    }
    string tempString = jk;
    file_vec.push_back(tempString); 
  }
  fclose(pFile);
#else
  //pFile = fopen("/home/jingyuq/mySummerResearch/Dedos_tmp/input_cl2.json", "r");
  pFile = fopen("/home/jingyuq/mySummerResearch/Dedos_tmp/test_copy2.json", "r");
  for (int line = 0; line < 185; line++) {
    fgets(jk, 100, pFile);
    string tempString = jk;
    file_vec.push_back(tempString); 
  }
  fclose(pFile); // this block for the second data graph flow.
#endif

  log_critical("Created %s MSU with id: %u", self->type->name, self->id);
  return 0;
}

void print_recv_msu_global_state() {
  uint64_t diff;
  diff = BILLION * (recv_msu_global_state->te.tv_sec - recv_msu_global_state->ts.tv_sec) + recv_msu_global_state->te.tv_nsec - recv_msu_global_state->ts.tv_nsec;
  double throughput = recv_msu_global_state->count * BILLION * 1.0f / diff;
  log_critical("Latency: %" PRIu64 " nsec\n", diff);
  log_critical("received %" PRIu32 " packets -> %f pkts/sec\n", recv_msu_global_state->count, throughput);
  /*ofstream oFile;
  oFile.open("/home/jingyuq/mySummerResearch/Dedos_tmp/nd_client/testResult.txt", ios_base::app);
  //if (!oFile.is_open()) 
  //  printf("fail open file\n");
  oFile << throughput << "\t" << recv_msu_global_state->count << "\t\n";
  oFile.close();
  oFile.open("/home/jingyuq/mySummerResearch/Dedos_tmp/nd_client/testResult_y.txt", ios_base::app);
  oFile << throughput << "\n";
  oFile.close();*/
} // This function do the same as ndlog_recv_msu_destroy.

void ndlog_recv_msu_destroy(struct local_msu *self)
{
  //report statistics
  printf("start destroy\n");
  uint64_t diff;
  recv_state* s = (recv_state*)(self->msu_state);
  diff = BILLION * (s->te.tv_sec - s->ts.tv_sec) + s->te.tv_nsec - s->ts.tv_nsec;
  double throughput = s->count * BILLION * 1.0f / diff;
  log_critical("received %" PRIu32 " packets -> %f pkts/sec\n", s->count, throughput);
  printf("end destroy\n");
}

struct msu_type NDLOG_RECV_MSU_TYPE = {
    .name=(char*)"ndlog_recv_msu",
    .id=NDLOG_RECV_MSU_TYPE_ID,
    .init_type=NULL,
    .destroy_type=NULL,
    .init=ndlog_recv_msu_init,
    .destroy=ndlog_recv_msu_destroy,
    .receive=ndlog_recv_msu_receive,
    .route=NULL,
    .serialize=NULL,
    .deserialize=NULL
};
