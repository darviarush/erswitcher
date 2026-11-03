#ifndef __ERSWITCHER_INFRASTRUCTURE_X11_EVENTS__H__
#define __ERSWITCHER_INFRASTRUCTURE_X11_EVENTS__H__

#include <X11/Xlib.h>

#include "../../ports/event_loop.h"

typedef struct {
    Display* display;
} event_loop_x11_t;

extern event_loop_x11_t er_event_loop;

void event_loop_x11_port(event_loop_port_t* port, event_loop_x11_t* state);

#endif /* __ERSWITCHER_INFRASTRUCTURE_X11_EVENTS__H__ */
