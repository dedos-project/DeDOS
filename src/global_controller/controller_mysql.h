#ifndef CONTROLLER_MYSQL_H
#define CONTROLLER_MYSQL_H

#include "mysql.h"
#include "dfg.h"
#include "stats.h"

extern MYSQL mysql;

int db_init(int clear);
int db_terminate();

int db_check_and_register(const char *check_query, const char *insert_query,
                          const char *element, int thread_id);
int db_register_runtime(int runtime_id);
int db_register_thread(int thread_id, int runtime_id);
int db_register_msu_stats(struct dfg_msu *msu, int thread_id, int runtime_id);
int db_register_msu(struct dfg_msu *msu, int thread_id, int runtime_id);
int db_register_timeseries(struct dfg_msu *msu, int thread_id, int runtime_id);

int db_insert_sample(struct timed_stat *input, struct stat_sample_hdr *input_hdr);

#endif
