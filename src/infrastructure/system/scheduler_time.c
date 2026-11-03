#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "scheduler_time.h"

scheduler_system_t er_scheduler;

static scheduler_system_t* state_of(scheduler_port_t* port) {
    return (scheduler_system_t*) port->context;
}

static double now_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
}

static int after_impl(scheduler_port_t* port, double seconds,
                      scheduler_callback_t callback, void* callback_context) {
    scheduler_system_t* state = state_of(port);

    int i;
    for(i = 0; i < SCHEDULER_MAX_TIMERS; i++) {
        if(state->timers[i].fn == NULL) break;
    }

    if(i == SCHEDULER_MAX_TIMERS) {
        fprintf(stderr, "Очередь таймеров переполнена. Установка таймера не удалась\n");
        return -1;
    }

    state->timers[i].fn = callback;
    state->timers[i].context = callback_context;
    state->timers[i].time = now_seconds() + seconds;

    if(state->size <= i) state->size = i + 1;

    return i;
}

static void tick_impl(scheduler_port_t* port) {
    scheduler_system_t* state = state_of(port);
    double now = now_seconds();

    int last = 0;
    for(int i = 0; i < state->size; i++) {
        if(state->timers[i].fn == NULL) continue;

        if(state->timers[i].time < now) {
            scheduler_callback_t fn = state->timers[i].fn;
            void* context = state->timers[i].context;

            if(state->debug) fprintf(stderr, "fn by timer %i\n", i);

            fn(context);

            // Слот освобождаем после callback: новый таймер, поставленный
            // из callback, не должен занять его раньше времени.
            state->timers[i].fn = NULL;
            state->timers[i].context = NULL;
        }
        else last = i;
    }

    state->size = last + 1;
}

void scheduler_system_port(scheduler_port_t* port, scheduler_system_t* state) {
    memset(port, 0, sizeof(*port));
    port->context = state;
    port->after = after_impl;
    port->tick = tick_impl;
}

void scheduler_system_reset(scheduler_system_t* state) {
    memset(state, 0, sizeof(*state));
}
