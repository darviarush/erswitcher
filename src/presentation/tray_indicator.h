#ifndef __ERSWITCHER_PRESENTATION_TRAY_INDICATOR__H__
#define __ERSWITCHER_PRESENTATION_TRAY_INDICATOR__H__

#include "../ports/tray.h"

/*
 * Состояние иконки в лотке поверх ports/tray.
 *
 * Клик не запускает конфигуратор напрямую: сценарий приложения передаёт
 * callback, а конкретный запуск процесса остаётся в infrastructure/system.
 */
typedef struct {
    tray_port_t* tray;
    void (*on_click)(void* context);
    void* click_context;
    int width;
    int height;
} tray_indicator_t;

void tray_indicator_init(tray_indicator_t* self, tray_port_t* tray,
                         void (*on_click)(void* context),
                         void* click_context);

int tray_indicator_attach(tray_indicator_t* self, void* display,
                          unsigned long window);

/* Возвращает 1, если событие относилось к лотку. */
int tray_indicator_handle_event(tray_indicator_t* self,
                                const void* native_event);

void tray_indicator_size(tray_indicator_t* self, int* width, int* height);

#endif /* __ERSWITCHER_PRESENTATION_TRAY_INDICATOR__H__ */
