#ifndef __ERSWITCHER_DOMAIN_TEXT__H__
#define __ERSWITCHER_DOMAIN_TEXT__H__

#include <wchar.h>

/*
 * Работа с UTF-8 текстом без привязки к X11 и I/O
 */

// Количество кодовых точек в UTF-8 строке (не байт)
int utf8_strlen(const char* s);

// Декодирует один UTF-8 символ. Возвращает число байт (1..4),
// 0 в конце строки или -1 при некорректной последовательности
int utf8_decode(const char* s, wint_t* out);

#endif /* __ERSWITCHER_DOMAIN_TEXT__H__ */
