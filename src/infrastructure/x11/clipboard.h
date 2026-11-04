#ifndef __ERSWITCHER_INFRASTRUCTURE_X11_CLIPBOARD__H__
#define __ERSWITCHER_INFRASTRUCTURE_X11_CLIPBOARD__H__

#include <X11/Xlib.h>

#include "../../ports/clipboard.h"

/*
 * Состояние X11-адаптера буфера обмена.
 *
 * Здесь же лежат Selection/INCR-буферы и ожидающий get()-callback. Монолит
 * на время переноса обращается к ним через compatibility-макросы, но новые
 * сценарии должны использовать clipboard_port.
 */
typedef struct {
    Display* display;
    Window window;

    Atom sel_data_atom;
    Atom utf8_atom;
    Atom clipboard_atom;
    Atom targets_atom;
    Atom incr_atom;

    int selection_chunk_size;

    char* send_data;
    char* send_pos;
    int send_len;
    Atom send_target;

    char* receive_data;
    int receive_len;
    Atom receive_type;

    clipboard_get_callback_t pending_callback;
    void* pending_context;

    int incr_is;
    Atom incr_property;
    Window incr_window;
} clipboard_x11_t;

extern clipboard_x11_t er_clipboard;

// Запоминает display/окно и создаёт атомы. Вызывается из open_display()
void clipboard_x11_init(clipboard_x11_t* state, Display* display, Window window);

// Заполняет порт колбэками, работающими с state
void clipboard_x11_port(clipboard_port_t* port, clipboard_x11_t* state);

// Обрабатывает SelectionNotify/SelectionClear/SelectionRequest/PropertyNotify
// Возвращает 1, если событие относилось к буферу обмена
int clipboard_x11_handle_event(clipboard_x11_t* state, XEvent* event);

// Сбрасывает данные и ожидающий callback (для тестов и завершения)
void clipboard_x11_reset(clipboard_x11_t* state);

#endif /* __ERSWITCHER_INFRASTRUCTURE_X11_CLIPBOARD__H__ */
