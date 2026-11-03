#include <stddef.h>

#include "sendkeys.h"
#include "../../domain/text/text.h"

void sendkeys_use_case_run(keyboard_port_t* keyboard, const char* text) {
    if(!keyboard || !text) return;

    keyboard->clear_active_mods(keyboard);

    const char* s = text;
    while(*s) {
        wint_t symbol = 0;
        int len = utf8_decode(s, &symbol);
        if(len <= 0) break;

        unikey_t key = keyboard->from_symbol(keyboard, (uint32_t) symbol);
        keyboard->press_key(keyboard, key);
        s += len;
    }

    keyboard->recover_active_mods(keyboard);
}
