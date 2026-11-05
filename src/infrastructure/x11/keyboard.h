#ifndef __ERSWITCHER_INFRASTRUCTURE_X11_KEYBOARD__H__
#define __ERSWITCHER_INFRASTRUCTURE_X11_KEYBOARD__H__

#include <X11/Xlib.h>
#include <stddef.h>

#include "../../ports/keyboard.h"

/*
 * Состояние X11-адаптера клавиатуры
 *
 * display и раскладки — инфраструктурные данные. Буферы saved/keys и
 * active_* — состояние опроса и зажатых модификаторов. Домен об этом
 * ничего не знает
 */
typedef struct {
    Display* display;

    int groups;
    int group_ru;
    int group_en;
    int maybe_group;

    int delay_us;

    char buffer1[KEYBOARD_BUFFER_SIZE];
    char buffer2[KEYBOARD_BUFFER_SIZE];
    char* saved;
    char* keys;

    int active_codes[KEYBOARD_SIZE];
    int active_len;
    unikey_t active_state;
    int active_use;

    unikey_t switch_key;
    int switch_key_set;
    int os_switch_active;
} keyboard_x11_t;

// Единственный экземпляр на процесс; монолит и тесты видят его через
// временные макросы совместимости
extern keyboard_x11_t er_kbd;

// Заполняет порт колбэками, работающими с state
void keyboard_x11_port(keyboard_port_t* port, keyboard_x11_t* state);

// Форматирует модификаторы и имя клавиши в буфер
int keyboard_x11_format_sym(char* out, size_t size, int mods, uint32_t keysym);

// Отладочная печать модификаторов и имени клавиши
void keyboard_x11_print_sym(int mods, uint32_t keysym);

#endif /* __ERSWITCHER_INFRASTRUCTURE_X11_KEYBOARD__H__ */
