#include <stdio.h>

#include "tray_indicator.h"

static void tray_indicator_update_size(tray_indicator_t* self) {
    self->tray->size(self->tray, &self->width, &self->height);
}

void tray_indicator_init(tray_indicator_t* self, tray_port_t* tray,
                         void (*on_click)(void* context),
                         void* click_context) {
    self->tray = tray;
    self->on_click = on_click;
    self->click_context = click_context;
    self->width = -1;
    self->height = -1;
}

int tray_indicator_attach(tray_indicator_t* self, void* display,
                          unsigned long window) {
    if(!self || !self->tray || !self->tray->dock) return 0;

    int result = self->tray->dock(self->tray, display, window);
    tray_indicator_update_size(self);
    return result;
}

int tray_indicator_handle_event(tray_indicator_t* self,
                                const void* native_event) {
    if(!self || !self->tray || !self->tray->event) return 0;

    switch(self->tray->event(self->tray, native_event)) {
    case TRAY_EVENT_DOCKED:
        tray_indicator_update_size(self);
        printf("win = %i x %i\n", self->width, self->height);
        return 1;

    case TRAY_EVENT_CLICK:
        if(self->on_click) self->on_click(self->click_context);
        return 1;

    case TRAY_EVENT_RESIZED:
        tray_indicator_update_size(self);
        printf("width: %i height: %i\n", self->width, self->height);
        fflush(stdout);
        return 1;

    case TRAY_EVENT_EXPOSED:
        return 1;

    case TRAY_EVENT_NONE:
        break;
    }

    return 0;
}

void tray_indicator_size(tray_indicator_t* self, int* width, int* height) {
    if(width) *width = self->width;
    if(height) *height = self->height;
}
