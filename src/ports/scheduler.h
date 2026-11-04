#ifndef __ERSWITCHER_PORTS_SCHEDULER__H__
#define __ERSWITCHER_PORTS_SCHEDULER__H__

/*
 * Порт отложенных вызовов
 *
 * Сценарии не должны знать, чем меряется время и как хранится очередь
 * таймеров. after() ставит одноразовый callback, tick() выполняет все
 * созревшие таймеры. Возврат after() — идентификатор таймера или -1,
 * если очередь переполнена
 */

typedef struct scheduler_port scheduler_port_t;

typedef void (*scheduler_callback_t)(void* context);

struct scheduler_port {
    void* context;

    int (*after)(scheduler_port_t* self, double seconds,
                 scheduler_callback_t callback, void* callback_context);
    void (*tick)(scheduler_port_t* self);
};

#endif /* __ERSWITCHER_PORTS_SCHEDULER__H__ */
