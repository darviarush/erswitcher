#include "test.h"

TestSuite(scheduler, .init = er_fixture, .fini = er_fixture_teardown);

/*
 * Системный scheduler: проверяем контракт порта без X11
 * Время здесь настоящее, поэтому просроченные таймеры ставим с
 * отрицательной задержкой, а будущие — с большим запасом
 */

static void er_scheduler_callback(void* context) {
    (*(int*) context)++;
}

Test(scheduler, er_scheduler_runs_due_timer_once) {
    scheduler_system_reset(&er_scheduler);
    scheduler_port_t port;
    scheduler_system_port(&port, &er_scheduler);

    int calls = 0;
    ck_assert_int_ge(port.after(&port, -1.0, er_scheduler_callback, &calls), 0);
    ck_assert_int_eq(calls, 0);

    port.tick(&port);
    ck_assert_int_eq(calls, 1);

    // Таймер одноразовый: повторный tick его уже не выполняет
    port.tick(&port);
    ck_assert_int_eq(calls, 1);
}
Test(scheduler, er_scheduler_keeps_future_timer) {
    scheduler_system_reset(&er_scheduler);
    scheduler_port_t port;
    scheduler_system_port(&port, &er_scheduler);

    int calls = 0;
    ck_assert_int_ge(port.after(&port, 60.0, er_scheduler_callback, &calls), 0);

    port.tick(&port);
    ck_assert_int_eq(calls, 0);
    ck_assert_int_gt(er_scheduler.size, 0);
}
Test(scheduler, er_scheduler_passes_context_to_each_timer) {
    scheduler_system_reset(&er_scheduler);
    scheduler_port_t port;
    scheduler_system_port(&port, &er_scheduler);

    int first = 0;
    int second = 0;
    ck_assert_int_ge(port.after(&port, -1.0, er_scheduler_callback, &first), 0);
    ck_assert_int_ge(port.after(&port, -1.0, er_scheduler_callback, &second), 0);

    port.tick(&port);
    ck_assert_int_eq(first, 1);
    ck_assert_int_eq(second, 1);
}
