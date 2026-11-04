#include "test.h"

TestSuite(load_config, .init = er_fixture, .fini = er_fixture_teardown);

/*
 * Секции конфига, которые настраивают поведение без комбинаций клавиш —
 * [options] и общая оболочка разбора
 *
 * [functions] проверяется в test-functions.c, [compose] — в test-compose.c,
 * [sendkeys] — в test-sendkeys.c, [snippets] — в test-snippets.c,
 * [commands] — в test-commands.c
 */

// @category [options]

Test(load_config, er_options_delay_in_seconds) {
    er_load(
        ER_MINIMAL_OPTIONS
        "command_timeout=13,20\n"
        "command_interval=0,1\n");

    ck_assert_int_eq(er_kbd.delay_us, 10000);
    ck_assert_int_eq(loop_delay, 20000);
    ck_assert_double_eq_tol(command_timeout, 13.2, 1e-6);
    ck_assert_double_eq_tol(command_interval, 0.1, 1e-6);
}
Test(load_config, er_options_are_read_in_seconds) {
    // 500 — это 500 секунд, а не 500 микросекунд
    er_load("[options]\ndelay=500\n");
    ck_assert_int_eq(er_kbd.delay_us, 500 * 1000000);
}
Test(load_config, er_options_invalid_delay_falls_back_to_default) {
    er_load("[options]\ndelay=abc\n");
    ck_assert_int_eq(er_kbd.delay_us, DELAY);
}
Test(load_config, er_options_zero_delay_falls_back_to_default) {
    er_load("[options]\ndelay=0\n");
    ck_assert_int_eq(er_kbd.delay_us, DELAY);
}
Test(load_config, er_options_unknown_option_is_skipped) {
    er_load(
        "[options]\n"
        "no_such_option=1\n"
        "delay=0,5\n");

    ck_assert_int_eq(er_kbd.delay_us, 500000);
    ck_assert_int_eq(loop_delay, DELAY);
}
Test(load_config, er_options_not_set_keep_current_values) {
    // Опции разложены по глобалам и накапливаются: повторный load_config()
    // без [options] не сбрасывает прошлые значения. Тест фиксирует это
    // поведение — если оно изменится, тест должен упасть и напомнить
    er_load(ER_MINIMAL_OPTIONS);
    ck_assert_int_eq(er_kbd.delay_us, 10000);

    er_load("[options]\nloop_delay=0,5\n");
    ck_assert_int_eq(er_kbd.delay_us, 10000);
    ck_assert_int_eq(loop_delay, 500000);
}
// @category общая оболочка разбора

Test(load_config, er_config_missing_creates_default) {
    ck_assert_int_eq(access(er_env.config, F_OK), -1);

    load_config();

    // Файл создан, и в нём есть хоть что-то распознанное
    ck_assert_int_eq(access(er_env.config, F_OK), 0);
    ck_assert_int_gt(keyfn_size, 0);
}
Test(load_config, er_config_unknown_section_is_skipped) {
    er_load(
        "[no_such_section]\n"
        "Pause=word translate\n"
        "[functions]\n"
        "Pause=word translate\n");

    ck_assert_int_eq(keyfn_size, 1);
}
Test(load_config, er_config_line_without_equal_is_skipped) {
    er_load(
        "[functions]\n"
        "Pause word translate\n"
        "Pause=word translate\n");

    ck_assert_int_eq(keyfn_size, 1);
}
Test(load_config, er_config_comments_and_spaces) {
    er_load(
        "# комментарий\n"
        "   # тоже комментарий\n"
        "\n"
        "[functions]\n"
        "Pause=word translate   \n");

    ck_assert_int_eq(keyfn_size, 1);
}
Test(load_config, er_config_last_section_wins) {
    er_load(
        "[functions]\n"
        "Pause=word translate\n"
        "[options]\n"
        "delay=0,5\n"
        "[functions]\n"
        "Control+Pause=text translate\n");

    ck_assert_int_eq(keyfn_size, 2);
    ck_assert_int_eq(er_kbd.delay_us, 500000);
}
