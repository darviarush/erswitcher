#ifndef __ERSWITCHER_PORTS_KEYBOARD__H__
#define __ERSWITCHER_PORTS_KEYBOARD__H__

#include <stdint.h>

#include "../domain/unikey/unikey.h"

/*
 * Порт клавиатуры.
 *
 * Домен и сценарии не должны знать про X11, Xkb и XTest. Порт описывает
 * только операции: состояние клавиши, преобразования, раскладки, инъекцию
 * и сохранение/восстановление зажатых модификаторов.
 *
 * Реализация — infrastructure/x11/keyboard.c. В тестах слой X11 заменяется
 * моком, поэтому те же обёртки работают без живого X-сервера.
 */

#define KEYBOARD_SIZE (32 * 8)
#define KEYBOARD_BUFFER_SIZE (KEYBOARD_SIZE / 8)

// Значения совпадают с X11 KeySym: на первом этапе это снижает риск.
// Сценарии используют их через порт и не включают X11-заголовки.
#define KEYBOARD_SYMBOL_BACKSPACE 0xff08u
#define KEYBOARD_SYMBOL_SHIFT_L 0xffe1u
#define KEYBOARD_SYMBOL_INSERT 0xff63u

typedef enum {
    KEYBOARD_RELEASE = 0,
    KEYBOARD_PRESS = 1,
    KEYBOARD_PRESS_AND_RELEASE = 2,
} keyboard_event_t;

typedef enum {
    KEYBOARD_KEY_OTHER = 0,
    KEYBOARD_KEY_BACKSPACE,
    KEYBOARD_KEY_CONTROL,
    KEYBOARD_KEY_PRINTABLE,
} keyboard_key_kind_t;

typedef struct keyboard_port keyboard_port_t;

struct keyboard_port {
    void* context;

    void (*init)(keyboard_port_t* self);
    unikey_t (*state)(keyboard_port_t* self, int code);
    uint32_t (*to_keysym)(keyboard_port_t* self, unikey_t key);
    unikey_t (*from_keysym)(keyboard_port_t* self, uint32_t keysym);
    uint32_t (*to_symbol)(keyboard_port_t* self, unikey_t key);
    unikey_t (*from_symbol)(keyboard_port_t* self, uint32_t symbol);
    keyboard_key_kind_t (*kind)(keyboard_port_t* self, unikey_t key);

    int (*layout_count)(keyboard_port_t* self);
    int (*layout_ru)(keyboard_port_t* self);
    int (*layout_en)(keyboard_port_t* self);
    int (*layout_hint)(keyboard_port_t* self);
    void (*set_layout_hint)(keyboard_port_t* self, int group);
    void (*set_layout)(keyboard_port_t* self, int group);
    // Группа, которая будет восстановлена после recover_active_mods().
    void (*set_active_group)(keyboard_port_t* self, int group);

    void (*set_delay)(keyboard_port_t* self, int microseconds);
    void (*send_key)(keyboard_port_t* self, unikey_t key, keyboard_event_t event);
    void (*press_key)(keyboard_port_t* self, unikey_t key);
    void (*press_key_multi)(keyboard_port_t* self, unikey_t key, int n);
    void (*clear_active_mods)(keyboard_port_t* self);
    void (*recover_active_mods)(keyboard_port_t* self);

    // Текущая и предыдущая карты нажатых клавиш (KEYBOARD_BUFFER_SIZE байт).
    // poll() обновляет текущую карту, poll_saved() — предыдущую.
    void (*poll)(keyboard_port_t* self);
    void (*poll_saved)(keyboard_port_t* self);
    const char* (*keymap)(keyboard_port_t* self);
    const char* (*keymap_saved)(keyboard_port_t* self);
    void (*swap_keymap)(keyboard_port_t* self);
};

#endif /* __ERSWITCHER_PORTS_KEYBOARD__H__ */
