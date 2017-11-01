#ifndef RT_STATS_H_
#define RT_STATS_H_
#include "stats.h"

#include <stdbool.h>

//TODO: docstring
int remove_stat_item(enum stat_id stat_id, unsigned int item_id);

//TODO: docstring
int init_stat_item(enum stat_id stat_id, unsigned int item_id);

//TODO: docstring
int init_statistics();

// TODO: docstring
void finalize_statistics(char *statlog);

/** Starts a measurement of elapsed time.  */
int record_start_time(enum stat_id stat_id, unsigned int item_id);

/** Records elapsed time since previous call to record_start_time */
int record_end_time(enum stat_id stat_id, unsigned int item_id);

/** Increments the given statistic by the provided value */
int increment_stat(enum stat_id stat_id, unsigned int item_id, double value);

/** Records a statistic in the statlog */
int record_stat(enum stat_id stat_id, unsigned int item_id, double stat, bool relog);

struct stat_sample *get_stat_samples(enum stat_id stat_id, int *n_sample_out);
#endif

