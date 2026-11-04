#ifndef __ERSWITCHER_PORTS_CLIPBOARD__H__
#define __ERSWITCHER_PORTS_CLIPBOARD__H__

/*
 * Порт буфера обмена
 *
 * Контракт асинхронный: результат get() может прийти позже, в том числе
 * после нескольких событий INCR. Данные, переданные в callback, принадлежат
 * порту и живут только до возврата из callback. Строка set() переходит в
 * собственность порта
 *
 * Сценарии не должны знать про X11, Atom, Selection и INCR: это детали
 * infrastructure/x11/clipboard.c
 */

typedef enum {
    CLIPBOARD_SELECTION_CLIPBOARD = 0,
} clipboard_selection_t;

typedef enum {
    CLIPBOARD_FORMAT_UTF8 = 0,
} clipboard_format_t;

typedef struct clipboard_port clipboard_port_t;

/* data всегда не NULL; пустой буфер приходит как "" */
typedef void (*clipboard_get_callback_t)(void* context, const char* data, int len,
                                         clipboard_format_t format);

struct clipboard_port {
    void* context;

    void (*get)(clipboard_port_t* self, clipboard_selection_t selection,
                clipboard_format_t format, clipboard_get_callback_t callback,
                void* callback_context);

    /* s должна быть выделена malloc(): порт освободит её при замене/clear */
    void (*set)(clipboard_port_t* self, char* data, int len,
                clipboard_format_t format);

    void (*clear)(clipboard_port_t* self);
};

#endif /* __ERSWITCHER_PORTS_CLIPBOARD__H__ */
