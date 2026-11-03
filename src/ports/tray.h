#ifndef __ERSWITCHER_PORTS_TRAY__H__
#define __ERSWITCHER_PORTS_TRAY__H__

/*
 * Порт системного лотка (systray).
 *
 * Точка входа не должна знать про X11: она отдаёт порту непрозрачный
 * native event и получает одно из событий лотка. Отрисовка/состояние
 * иконки и реакция на клик живут в presentation/tray_indicator.
 */

typedef struct tray_port tray_port_t;

typedef enum {
    TRAY_EVENT_NONE = 0,
    TRAY_EVENT_DOCKED,
    TRAY_EVENT_CLICK,
    TRAY_EVENT_RESIZED,
    TRAY_EVENT_EXPOSED,
} tray_event_t;

#define TRAY_MESSAGE_REQUEST_DOCK 0
#define TRAY_MESSAGE_BEGIN 1
#define TRAY_MESSAGE_CANCEL 2

struct tray_port {
    void* context;

    /* Пристыковать native-окно к лотку. display/window — непрозрачные
     * для сценариев; X11-адаптер приводит их к своим типам. */
    int (*dock)(tray_port_t* self, void* display, unsigned long window);

    /* Отправить systray-сообщение (dock/message). */
    int (*send)(tray_port_t* self, long message,
                long data1, long data2, long data3);

    /* Разобрать native event. Возвращает NONE, если событие не про лоток. */
    tray_event_t (*event)(tray_port_t* self, const void* native_event);

    /* Размер пристыкованного окна; -1, пока размер неизвестен. */
    void (*size)(tray_port_t* self, int* width, int* height);
};

#endif /* __ERSWITCHER_PORTS_TRAY__H__ */
