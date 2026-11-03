#ifndef __ERSWITCHER_USE_CASES_TRANSLATE__H__
#define __ERSWITCHER_USE_CASES_TRANSLATE__H__

#include "../../domain/unikey/unikey.h"
#include "../../ports/keyboard.h"

/*
 * Сценарии перевода/инверсии регистра (вариант А).
 *
 * Строка не перекодируется: буфер unikey_t повторно нажимается в другой
 * группе раскладки. Активные модификаторы сохраняются и восстанавливаются
 * через порт клавиатуры.
 */
void translate_use_case_buffer(keyboard_port_t* keyboard, unikey_t* word, int pos,
                               int from, int backspace);
void translate_use_case_invertcase(keyboard_port_t* keyboard, unikey_t* word, int pos,
                                   int from, int backspace);

#endif /* __ERSWITCHER_USE_CASES_TRANSLATE__H__ */
