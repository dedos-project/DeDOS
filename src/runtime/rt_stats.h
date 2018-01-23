#ifndef _RT_BIN_STATS_H_
#define _RT_BIN_STATS_H_
#include "stats.h"

int record_msu_stat(enum stat_id sid, unsigned int id, double value);
int record_thread_stat(enum stat_id sid, unsigned int id, double value);
int record_rt_stat(enum stat_id sid, double value);

int increment_msu_stat(enum stat_id sid, unsigned int id, double increment);
int increment_thread_stat(enum stat_id sid, unsigned int id, double increment);
int increment_rt_stat(enum stat_id sid, double increment);

int last_msu_stat(enum stat_id sid, unsigned int id, double *out);
int last_thread_stat(enum stat_id sid, unsigned int id, double *out);
int last_rt_stat(enum stat_id sid, double *out);

int begin_msu_interval(enum stat_id sid, unsigned int id);
int begin_thread_interval(enum stat_id sid, unsigned int id);
int begin_rt_interval(enum stat_id sid);

int end_msu_interval(enum stat_id, unsigned int id);
int end_thread_interval(enum stat_id, unsigned int id);
int end_rt_interval(enum stat_id);

int remove_msu_stat(enum stat_id, unsigned int id);
int remove_thread_stat(enum stat_id, unsigned int id);
int remove_rt_stat(enum stat_id);

int init_msu_stat(enum stat_id, unsigned int id);
int init_thread_stat(enum stat_id, unsigned int id);
int init_rt_stat(enum stat_id);

int sample_stats(struct stat_sample **samples);

int check_statistics();

#endif
