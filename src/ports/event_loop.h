#ifndef __ERSWITCHER_PORTS_EVENT_LOOP__H__
#define __ERSWITCHER_PORTS_EVENT_LOOP__H__

/*
 * Порт цикла событий
 *
 * wait() ждёт событие или таймаут, drain() отдаёт все уже накопленные
 * события обработчику. Конкретный тип события принадлежит инфраструктуре:
 * приложение передаёт его в свой обработчик как native_event
 */

typedef struct event_loop_port event_loop_port_t;

typedef void (*event_loop_handler_t)(void* context, const void* native_event);

struct event_loop_port {
    void* context;

    void (*wait)(event_loop_port_t* self, double seconds);
    void (*drain)(event_loop_port_t* self, event_loop_handler_t handler, void* context);
};

#endif /* __ERSWITCHER_PORTS_EVENT_LOOP__H__ */
