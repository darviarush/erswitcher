#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "events.h"

event_loop_x11_t er_event_loop;

static event_loop_x11_t* state_of(event_loop_port_t* port) {
    return (event_loop_x11_t*) port->context;
}

static void wait_impl(event_loop_port_t* port, double seconds) {
    event_loop_x11_t* state = state_of(port);
    if(!state->display) return;

    struct timeval tv = {
        (int) seconds,
        (int) ((seconds - (int) seconds) * 1000000)};

    fd_set readset;
    FD_ZERO(&readset);
    int fd_display = ConnectionNumber(state->display);
    FD_SET(fd_display, &readset);
    select(fd_display + 1, &readset, NULL, NULL, &tv);
}

static void drain_impl(event_loop_port_t* port, event_loop_handler_t handler,
                       void* context) {
    event_loop_x11_t* state = state_of(port);
    if(!state->display) return;

    while(XPending(state->display)) {
        XEvent event;
        XNextEvent(state->display, &event);
        handler(context, &event);
    }
}

void event_loop_x11_port(event_loop_port_t* port, event_loop_x11_t* state) {
    memset(port, 0, sizeof(*port));
    port->context = state;
    port->wait = wait_impl;
    port->drain = drain_impl;
}
