#include "translate.h"

#include "../../domain/case-invert/case-invert.h"

void translate_use_case_buffer(keyboard_port_t* keyboard, unikey_t* word, int pos,
                               int from, int backspace) {
    keyboard->clear_active_mods(keyboard);

    // clear_active_mods() запомнил группу для восстановления. Для перевода
    // подменяем её на противоположную
    unikey_t state = keyboard->state(keyboard, 0);
    int en = keyboard->layout_en(keyboard);
    int ru = keyboard->layout_ru(keyboard);
    keyboard->set_active_group(keyboard, state.group == en ? ru : en);

    if(backspace) {
        unikey_t bs = keyboard->from_keysym(keyboard, KEYBOARD_SYMBOL_BACKSPACE);
        keyboard->press_key_multi(keyboard, bs, pos - from);
    }

    for(int i = from; i < pos; i++) {
        word[i].group = word[i].group == en ? ru : word[i].group == ru ? en
                                                                       : word[i].group;
        keyboard->press_key(keyboard, word[i]);
    }

    keyboard->recover_active_mods(keyboard);
}

void translate_use_case_invertcase(keyboard_port_t* keyboard, unikey_t* word, int pos,
                                   int from, int backspace) {
    keyboard->clear_active_mods(keyboard);

    if(backspace) {
        unikey_t bs = keyboard->from_keysym(keyboard, KEYBOARD_SYMBOL_BACKSPACE);
        keyboard->press_key_multi(keyboard, bs, pos - from);
    }

    for(int i = from; i < pos; i++) {
        word[i] = unikey_invert_shift(word[i]);
        keyboard->press_key(keyboard, word[i]);
    }

    keyboard->recover_active_mods(keyboard);
}
