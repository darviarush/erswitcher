#include "test.h"

/*
 * Системный scheduler: проверяем контракт порта без X11
 * Время здесь настоящее, поэтому просроченные таймеры ставим с
 * отрицательной задержкой, а будущие — с большим запасом
 */

static void er_scheduler_callback(void* context) {
    (*(int*) context)++;
}

START_TEST(er_scheduler_runs_due_timer_once) {
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
END_TEST

START_TEST(er_scheduler_keeps_future_timer) {
    scheduler_system_reset(&er_scheduler);
    scheduler_port_t port;
    scheduler_system_port(&port, &er_scheduler);

    int calls = 0;
    ck_assert_int_ge(port.after(&port, 60.0, er_scheduler_callback, &calls), 0);

    port.tick(&port);
    ck_assert_int_eq(calls, 0);
    ck_assert_int_gt(er_scheduler.size, 0);
}
END_TEST

START_TEST(er_scheduler_passes_context_to_each_timer) {
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
END_TEST

Suite* suite_scheduler() {
    Suite* s = suite_create("scheduler port");

    TCase* tc = tcase_create("system clock");
    tcase_add_checked_fixture(tc, er_fixture, er_fixture_teardown);
    tcase_add_test(tc, er_scheduler_runs_due_timer_once);
    tcase_add_test(tc, er_scheduler_keeps_future_timer);
    tcase_add_test(tc, er_scheduler_passes_context_to_each_timer);
    suite_add_tcase(s, tc);

    return s;
}
