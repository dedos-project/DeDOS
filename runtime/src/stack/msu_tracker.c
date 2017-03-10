#include "msu_tracker.h"
#include "logging.h"

#include "modules/dummy_msu.h"
#include "modules/regex_msu.h"
#include "modules/ssl_request_routing_msu.h"
#include "modules/ssl_read_msu.h"
#include "modules/ssl_write_msu.h"
#include "modules/webserver_msu.h"
#include "generic_msu.h"
#include "modules/hs_request_routing_msu.h"
#include "modules/msu_app_tcp_echo.h"
#include "modules/msu_pico_tcp.h"
#include "modules/msu_tcp_handshake.h"

#define N_MSU_TYPES 10

static msu_type *MSU_TYPES[] = {
    &SSL_REQUEST_ROUTING_MSU_TYPE,
    &DUMMY_MSU_TYPE,
    &REGEX_MSU_TYPE,
    &SSL_READ_MSU_TYPE,
    &SSL_WRITE_MSU_TYPE,
    &WEBSERVER_MSU_TYPE,
    &PICO_TCP_MSU_TYPE,
    &HS_REQUEST_ROUTING_MSU_TYPE,
    &MSU_APP_TCP_ECHO_TYPE,
    &TCP_HANDSHAKE_MSU_TYPE
};

void register_known_msu_types(){
    for (int i=0; i<N_MSU_TYPES; i++){
        register_msu_type(MSU_TYPES[i]);
    }
}


void msu_tracker_add(int msu_id, struct dedos_thread *dedos_thread) {
    struct msu_placement_tracker *s;

    s = malloc(sizeof(struct msu_placement_tracker));
    s->msu_id = msu_id;
    s->dedos_thread = dedos_thread;
    mutex_lock(msu_tracker->mutex);
    HASH_ADD_INT( msu_tracker->msu_placements, msu_id, s );
    mutex_unlock(msu_tracker->mutex);
}

struct msu_placement_tracker *msu_tracker_find(int msu_id) {
    struct msu_placement_tracker *s;
    mutex_lock(msu_tracker->mutex);
    HASH_FIND_INT( msu_tracker->msu_placements, &msu_id, s );  /* s: output pointer */
    mutex_unlock(msu_tracker->mutex);
    return s;
}

void msu_tracker_delete(struct msu_placement_tracker *item) {
	mutex_lock(msu_tracker->mutex);
	HASH_DEL(msu_tracker->msu_placements, item);  /* item: pointer to deletee */
	mutex_unlock(msu_tracker->mutex);
	free(item);
}

unsigned int msu_tracker_count(void){
    unsigned int num_msus;
    mutex_lock(msu_tracker->mutex);
    num_msus = HASH_COUNT(msu_tracker->msu_placements);
    mutex_unlock(msu_tracker->mutex);
    return num_msus;
}

void msu_tracker_print_all(void){
    struct msu_placement_tracker *s;
    log_info("Total MSUs on runtime: %d",msu_tracker_count());
    mutex_lock(msu_tracker->mutex);
    for(s=msu_tracker->msu_placements; s != NULL; s=(struct msu_placement_tracker*)(s->hh.next)) {
        log_info("\tMSU_ID: %d on thread: %lu", s->msu_id, s->dedos_thread->tid);
    }
    mutex_unlock(msu_tracker->mutex);
}

void msu_tracker_get_all_info(struct msu_thread_info *all_msus, unsigned int num){
    struct msu_placement_tracker *s;
    log_debug("INFO: Total MSUs on runtime: %d",msu_tracker_count());
    unsigned int j = 0;
    mutex_lock(msu_tracker->mutex);
    struct msu_thread_info *cur;
    for(s=msu_tracker->msu_placements; s != NULL; s=(struct msu_placement_tracker*)(s->hh.next), j < num) {
        log_debug("\tMSU_ID: %d on thread: %lu", s->msu_id, s->dedos_thread->tid);
        cur = &all_msus[j];
        cur->msu_id = s->msu_id;
        cur->thread_id = s->dedos_thread->tid;
        j++;
    }
    mutex_unlock(msu_tracker->mutex);
}

void msu_tracker_destroy(void){
    struct msu_placement_tracker *current_entry, *tmp;

    mutex_lock(msu_tracker->mutex);
    HASH_ITER(hh, msu_tracker->msu_placements, current_entry, tmp) {
        HASH_DEL(msu_tracker->msu_placements, current_entry);
        free(current_entry);
    }
    mutex_unlock(msu_tracker->mutex);

    if(msu_tracker->msu_placements != NULL){
        free(msu_tracker->msu_placements);
    }
    mutex_destroy(msu_tracker->mutex);
    free(msu_tracker);
    log_debug("Destroyed msu_tracker %s","");
}
