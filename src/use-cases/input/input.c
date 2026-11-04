#include "input.h"

void input_use_case_init(input_use_case_t* self, input_buffer_t* buffer,
                         keyboard_port_t* keyboard) {
    self->buffer = buffer;
    self->keyboard = keyboard;
}

void input_use_case_reset(input_use_case_t* self) {
    input_buffer_reset(self->buffer);
}

void input_use_case_push(input_use_case_t* self, unikey_t key) {
    wint_t symbol = (wint_t) self->keyboard->to_symbol(self->keyboard, key);
    input_buffer_push(self->buffer, key, symbol);
}

void input_use_case_backspace(input_use_case_t* self) {
    input_buffer_backspace(self->buffer);
}

void input_use_case_handle_key(input_use_case_t* self, unikey_t key) {
    // Управляющие модификаторы не участвуют во вводе обычного текста
    if(key.mods & ~(ERS_MOD_SHIFT | ERS_MOD_LOCK | ERS_MOD_NUM)) return;

    keyboard_key_kind_t kind = self->keyboard->kind(self->keyboard, key);
    if(kind == KEYBOARD_KEY_BACKSPACE) {
        int mods = key.mods & ~(ERS_MOD_LOCK | ERS_MOD_NUM);
        if(mods == 0) input_use_case_backspace(self);
        return;
    }

    if(kind == KEYBOARD_KEY_CONTROL) {
        input_use_case_reset(self);
        return;
    }

    if(kind == KEYBOARD_KEY_PRINTABLE) {
        input_use_case_push(self, key);
    }
}

int input_use_case_from_space(const input_use_case_t* self) {
    return input_buffer_from_space(self->buffer);
}
