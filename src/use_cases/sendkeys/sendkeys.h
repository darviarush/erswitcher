#ifndef __ERSWITCHER_USE_CASES_SENDKEYS__H__
#define __ERSWITCHER_USE_CASES_SENDKEYS__H__

#include "../../ports/keyboard.h"

/*
 * Ввод UTF-8 строки эмуляцией нажатий.
 *
 * Сценарий не знает про KeySym/X11: каждый Unicode-символ превращается
 * в unikey_t через keyboard->from_symbol(), после чего нажимается через
 * keyboard->press_key(). Активные модификаторы сохраняются и
 * восстанавливаются, как в старом sendkeys().
 */
void sendkeys_use_case_run(keyboard_port_t* keyboard, const char* text);

#endif /* __ERSWITCHER_USE_CASES_SENDKEYS__H__ */
