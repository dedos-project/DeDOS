/**
 * @file rt_stats.h
 *
 * Collecting statistics within the runtime
 */

#ifndef RT_STATS_H_
#define RT_STATS_H_
#include "stats.h"

#include <stdbool.h>

/**
 * Un-registers an item so it can no longer have statistics registered,
 * and will not be reported to the global controller
 */
int remove_stat_item(enum stat_id stat_id, unsigned int item_id);

/**
 * Initializes a new stat item so it can have stats registered
 * and can be reported to the global controller
 */
int init_stat_item(enum stat_id stat_id, unsigned int item_id);

/**
 * Initializes the entire stats module. MUST BE CALLED before runtime starts
 */
int init_statistics();

/**
 * Writes the statistics to statlog if provided, and frees assocated structure
 * @param statlog Log file to dump statistics to or NULL if N/A
 */
void finalize_statistics(char *statlog);

/**
 * Starts a measurement of elapsed time.
 * Not added to the log until a call to record_end_time
 * @param stat_id ID for stat type being logged
 * @param item_id ID for the item to which the stat refers
 * @return 0 on success, -1 on error
 */
int record_start_time(enum stat_id stat_id, unsigned int item_id);

/**
 * Records the elapsed time since the previous call to record_start_time
 * @param stat_id ID for stat being logged
 * @param item_id ID for item within the statistic
 */
int record_end_time(enum stat_id stat_id, unsigned int item_id);

/**
 * Increments the given statistic by the provided value
 * @param stat_id ID for the stat being logged
 * @param item_id ID for the item to which the stat refers (must be registered!)
 * @param value The amount to add to the given stat
 * @return 0 on success, -1 on error
 */
int increment_stat(enum stat_id stat_id, unsigned int item_id, double value);

/**
 * Records a statistic in the statlog
 * @param stat_id ID for stat being logged
 * @param item_id ID for item to which stat refers (must be registered!)
 * @param stat Statistic to record
 * @param relog Whether to log statistic if it matches the previously logged stat
 * @return 0 on success, -1 on error
 */
int record_stat(enum stat_id stat_id, unsigned int item_id, double stat, bool relog);

/** Returns the last statistic recorded */
double get_last_stat(enum stat_id stat_id, unsigned int item_id);

/**
 * Samples the statistic with the provided stat_id.
 * @param stat_id the ID of the stat_type to sample
 * @param n_samples_out Output argument, stores the number of samples acquired
 * @return The statically-allocated stat sample (does not need to be freed)
 */
struct stat_sample *get_stat_samples(enum stat_id stat_id, int *n_sample_out);
#endif

