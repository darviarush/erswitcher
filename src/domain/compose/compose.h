#ifndef __ERSWITCHER_DOMAIN_COMPOSE__H__
#define __ERSWITCHER_DOMAIN_COMPOSE__H__

#include <wchar.h>

/*
 * Мнемоники [compose]: поиск самого длинного совпадения в конце буфера.
 * Модуль чистый: не вставляет текст и не нажимает клавиши.
 */

#define COMPOSE_KEY_MAX 20

typedef struct {
    wint_t word[COMPOSE_KEY_MAX];   // левая часть (что ввели)
    int pos;                        // длина левой части
    int cursor_back;                // на сколько вернуть курсор после вставки
    char* to;                       // правая часть (что вставить)
} compose_t;

// Индекс первого совпадения в map[0..size) или -1.
// symbols — Unicode-символы буфера, symbols_pos — длина ввода.
int compose_find(const compose_t* map, int size, const wint_t* symbols, int symbols_pos);

#endif /* __ERSWITCHER_DOMAIN_COMPOSE__H__ */
