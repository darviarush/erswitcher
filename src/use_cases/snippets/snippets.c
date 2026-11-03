#include <stdlib.h>
#include <string.h>

#include "snippets.h"

static void snippets_after_shift_insert(void* context) {
    snippets_use_case_t* self = (snippets_use_case_t*) context;

    keyboard_port_t* keyboard = self->keyboard;
    keyboard->clear_active_mods(keyboard);

    unikey_t shift = keyboard->from_keysym(keyboard, KEYBOARD_SYMBOL_SHIFT_L);
    unikey_t insert = keyboard->from_keysym(keyboard, KEYBOARD_SYMBOL_INSERT);

    keyboard->send_key(keyboard, shift, KEYBOARD_PRESS);
    keyboard->press_key(keyboard, insert);
    keyboard->send_key(keyboard, shift, KEYBOARD_RELEASE);

    keyboard->recover_active_mods(keyboard);
    keyboard->set_layout_hint(keyboard, keyboard->state(keyboard, 0).group);
}

void snippets_use_case_init(snippets_use_case_t* self,
                             clipboard_port_t* clipboard,
                             scheduler_port_t* scheduler,
                             keyboard_port_t* keyboard) {
    self->clipboard = clipboard;
    self->scheduler = scheduler;
    self->keyboard = keyboard;
}

void snippets_use_case_insert(snippets_use_case_t* self, const char* text) {
    if(!self || !self->clipboard || !self->scheduler || !self->keyboard) return;

    char* copy = strdup(text ? text : "");
    if(!copy) return;

    self->clipboard->set(self->clipboard, copy, (int) strlen(copy),
                         CLIPBOARD_FORMAT_UTF8);
    self->scheduler->after(self->scheduler, 0.10,
                           snippets_after_shift_insert, self);
}
