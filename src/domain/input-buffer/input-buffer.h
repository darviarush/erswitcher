#ifndef __ERSWITCHER_DOMAIN_INPUT_BUFFER__H__
#define __ERSWITCHER_DOMAIN_INPUT_BUFFER__H__

#include <wchar.h>

#include "../unikey/unikey.h"

/*
 * Кольцевой буфер последнего ввода
 *
 * Для каждой клавиши хранится и unikey_t (сканкод/модификаторы/группа),
 * и Unicode-символ. Первое нужно для повторного нажатия при смене
 * раскладки, второе — для границ слова и поиска [compose]
 */

#define INPUT_BUFFER_CAPACITY 1024

typedef struct {
    int pos; // позиция в буфере
    unikey_t word[INPUT_BUFFER_CAPACITY]; // нажатые клавиши
    wint_t symbol[INPUT_BUFFER_CAPACITY]; // их Unicode-символы
} input_buffer_t;

void input_buffer_init(input_buffer_t* buffer);
void input_buffer_reset(input_buffer_t* buffer);
void input_buffer_push(input_buffer_t* buffer, unikey_t key, wint_t symbol);
void input_buffer_backspace(input_buffer_t* buffer);

// Начало последнего слова: после последнего пробельного символа
// Если пробелов нет — 0. Пустой буфер — тоже 0
int input_buffer_from_space(const input_buffer_t* buffer);

#endif /* __ERSWITCHER_DOMAIN_INPUT_BUFFER__H__ */
