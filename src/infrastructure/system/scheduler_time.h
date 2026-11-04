#ifndef __ERSWITCHER_INFRASTRUCTURE_SYSTEM_SCHEDULER_TIME__H__
#define __ERSWITCHER_INFRASTRUCTURE_SYSTEM_SCHEDULER_TIME__H__

#include "../../ports/scheduler.h"

/*
 * Системная реализация scheduler: gettimeofday + массив одноразовых
 * таймеров. Поведение и размер очереди совпадают со старым монолитом
 */

#define SCHEDULER_MAX_TIMERS 256

typedef struct {
    double time;
    scheduler_callback_t fn;
    void* context;
} scheduler_timer_t;

typedef struct {
    scheduler_timer_t timers[SCHEDULER_MAX_TIMERS];
    int size;
} scheduler_system_t;

extern scheduler_system_t er_scheduler;

void scheduler_system_port(scheduler_port_t* port, scheduler_system_t* state);
void scheduler_system_reset(scheduler_system_t* state);

#endif /* __ERSWITCHER_INFRASTRUCTURE_SYSTEM_SCHEDULER_TIME__H__ */
