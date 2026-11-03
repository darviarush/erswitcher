#include <string.h>

#include "tray.h"

static tray_x11_t* state_of(tray_port_t* port) {
    return (tray_x11_t*) port->context;
}

static int send_impl(tray_port_t* port, long message,
                     long data1, long data2, long data3) {
    tray_x11_t* state = state_of(port);
    if(!state->display) return 0;

    Display* d = state->display;

    Atom selection_atom = XInternAtom(d, "_NET_SYSTEM_TRAY_S0", False);
    Window tray = XGetSelectionOwner(d, selection_atom);

    if(tray != None) XSelectInput(d, tray, StructureNotifyMask);

    XEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.xclient.type = ClientMessage;
    ev.xclient.window = tray;
    ev.xclient.message_type = XInternAtom(d, "_NET_SYSTEM_TRAY_OPCODE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = message;
    ev.xclient.data.l[2] = data1;
    ev.xclient.data.l[3] = data2;
    ev.xclient.data.l[4] = data3;

    XSendEvent(d, tray, False, NoEventMask, &ev);
    XSync(d, False);
    return 1;
}

static int dock_impl(tray_port_t* port, void* display, unsigned long window) {
    tray_x11_t* state = state_of(port);
    state->display = (Display*) display;
    state->window = (Window) window;
    state->width = -1;
    state->height = -1;

    send_impl(port, TRAY_MESSAGE_REQUEST_DOCK, (long) window, 0, 0);
    return 1;
}

static tray_event_t event_impl(tray_port_t* port, const void* native_event) {
    tray_x11_t* state = state_of(port);
    const XEvent* event = (const XEvent*) native_event;
    if(!event) return TRAY_EVENT_NONE;

    switch(event->type) {
    case ClientMessage: {
        XWindowAttributes attr;
        if(state->display && state->window &&
           XGetWindowAttributes(state->display, state->window, &attr)) {
            state->width = attr.width;
            state->height = attr.height;
        }
        return TRAY_EVENT_DOCKED;
    }
    case ButtonPress:
        return TRAY_EVENT_CLICK;
    case ConfigureNotify:
        state->width = event->xconfigure.width;
        state->height = event->xconfigure.height;
        return TRAY_EVENT_RESIZED;
    case Expose:
        return TRAY_EVENT_EXPOSED;
    default:
        return TRAY_EVENT_NONE;
    }
}

static void size_impl(tray_port_t* port, int* width, int* height) {
    tray_x11_t* state = state_of(port);
    if(width) *width = state->width;
    if(height) *height = state->height;
}

void tray_x11_port(tray_port_t* port, tray_x11_t* state) {
    memset(port, 0, sizeof(*port));
    port->context = state;
    port->dock = dock_impl;
    port->send = send_impl;
    port->event = event_impl;
    port->size = size_impl;
}
