#ifndef __ERSWITCHER_INFRASTRUCTURE_X11_TRAY__H__
#define __ERSWITCHER_INFRASTRUCTURE_X11_TRAY__H__

#include <X11/Xlib.h>

#include "../../ports/tray.h"

/*
 * X11-реализация systray: _NET_SYSTEM_TRAY_S0, _NET_SYSTEM_TRAY_OPCODE,
 * докинг окна и разбор ClientMessage/ButtonPress/ConfigureNotify/Expose.
 */

typedef struct {
    Display* display;
    Window window;
    int width;
    int height;
} tray_x11_t;

void tray_x11_port(tray_port_t* port, tray_x11_t* state);

#endif /* __ERSWITCHER_INFRASTRUCTURE_X11_TRAY__H__ */
