#ifndef __ERSWITCHER_DOMAIN_CASE_INVERT__H__
#define __ERSWITCHER_DOMAIN_CASE_INVERT__H__

#include "../unikey/unikey.h"

/*
 * Инверсия регистра на уровне клавиши: переключает Shift-модификатор.
 * Именно так работает Alt+Pause в текущем приложении — строка не
 * перекодируется, меняется только способ повторного нажатия клавиш.
 */

unikey_t unikey_invert_shift(unikey_t key);

#endif /* __ERSWITCHER_DOMAIN_CASE_INVERT__H__ */
