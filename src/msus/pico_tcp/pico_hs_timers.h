#ifndef DEDOS_HS_TIMERS_H_
#define DEDOS_HS_TIMERS_H_

struct hs_timer
{
    void *arg;
    void (*timer)(pico_time timestamp, void *arg, void *timers);
};


//struct pico_timer_ref
struct hs_timer_ref
{
    pico_time expire;
    uint32_t id;
    struct hs_timer *tmr;
};

typedef struct hs_timer_ref hs_timer_ref;

HS_DECLARE_HEAP(hs_timer_ref, expire);



#endif /* DEDOS_HS_TIMERS_H_ */
