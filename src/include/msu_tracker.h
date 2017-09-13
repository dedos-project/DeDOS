#ifndef MSU_TRACKER_H_
#define MSU_TRACKER_H_
#include "runtime.h"
#include "uthash.h"
/* to keep track of placement of MSU inside main thread
 * Helpful to place messages when received over control socket
 * in to appropriate destination MSU queue
 */
struct msu_placement_tracker
{
    int msu_id;/* key */
    struct dedos_thread *dedos_thread;
    void *mutex;
    UT_hash_handle hh; /* makes this structure hashable */
};

// wrapper around placement_tracker to add a mutex
struct msu_tracker{
	struct msu_placement_tracker *msu_placements;
	void *mutex;
};

//for collecting msu id and placement info for sending to master
struct msu_thread_info {
    int msu_id;
    pthread_t thread_id;
};

struct msu_tracker *msu_tracker;

int init_msu_tracker(void);
void msu_tracker_add(int msu_id, struct dedos_thread *dedos_thread);
struct msu_placement_tracker *msu_tracker_find(int msu_id);
void msu_tracker_delete(struct msu_placement_tracker *item);
unsigned int msu_tracker_count(void);
void msu_tracker_print_all(void);
void msu_tracker_get_all_info(struct msu_thread_info *all_msus, unsigned int num);
void msu_tracker_destroy(void);
#endif /* MSU_TRACKER_H_ */
