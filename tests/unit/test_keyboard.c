#include "test.h"

/*
 * Порт клавиатуры. Здесь проверяется сам контракт, а не только старые
 * обёртки монолита: X11-адаптер подменён mock.c, поэтому тесты не требуют
 * живого X-сервера.
 */

static void er_keyboard_layout() {
    mock_keymap_clear();
    mock_keymap_set(KEY_A, 0, 0, XK_a);
    mock_keymap_set(KEY_A, 1, 0, XK_b);
    er_kbd.groups = 2;
    er_kbd.group_en = 0;
    er_kbd.group_ru = 1;
    er_kbd.maybe_group = -1;
    mock_keyboard(0);
    er_kbd.delay_us = 0;
}

START_TEST(er_keyboard_port_converts_symbols) {
    er_keyboard_layout();
    keyboard_port_ensure();

    ck_assert_int_eq(kbd_port.layout_count(&kbd_port), 2);
    ck_assert_int_eq(kbd_port.layout_en(&kbd_port), 0);
    ck_assert_int_eq(kbd_port.layout_ru(&kbd_port), 1);

    unikey_t key = {code : KEY_A, mods : 0, group : 0};
    ck_assert_int_eq(kbd_port.to_keysym(&kbd_port, key), XK_a);
    ck_assert_int_eq(kbd_port.to_symbol(&kbd_port, key), 'a');

    unikey_t found = kbd_port.from_symbol(&kbd_port, 'a');
    ck_assert_int_eq(found.code, KEY_A);
    ck_assert_int_eq(found.mods, 0);
    ck_assert_int_eq(found.group, 0);
}
END_TEST

START_TEST(er_keyboard_port_classifies_keys) {
    er_keyboard_layout();
    mock_keymap_set_all_groups(KEY_BACKSPACE, XK_BackSpace);
    mock_keymap_set_all_groups(KEY_LEFT, XK_Left);
    keyboard_port_ensure();

    ck_assert_int_eq(kbd_port.kind(&kbd_port, (unikey_t){code : KEY_A, mods : 0, group : 0}), KEYBOARD_KEY_PRINTABLE);
    ck_assert_int_eq(kbd_port.kind(&kbd_port, (unikey_t){code : KEY_BACKSPACE, mods : 0, group : 0}), KEYBOARD_KEY_BACKSPACE);
    ck_assert_int_eq(kbd_port.kind(&kbd_port, (unikey_t){code : KEY_LEFT, mods : 0, group : 0}), KEYBOARD_KEY_CONTROL);
    ck_assert_int_eq(kbd_port.kind(&kbd_port, (unikey_t){code : 255, mods : 0, group : 0}), KEYBOARD_KEY_OTHER);
}
END_TEST

START_TEST(er_keyboard_port_state_and_layout) {
    er_keyboard_layout();
    keyboard_port_ensure();

    mock_keyboard(1);
    mock_keyboard_mods_set(ShiftMask);
    unikey_t state = kbd_port.state(&kbd_port, KEY_A);
    ck_assert_int_eq(state.code, KEY_A);
    ck_assert_int_eq(state.group, 1);
    ck_assert_int_eq(state.mods, ShiftMask);

    mock_reset();
    kbd_port.set_layout(&kbd_port, 0);
    ck_assert_int_eq(kbd_port.state(&kbd_port, 0).group, 0);
    ck_assert_msg(mock_find("XkbLockGroup(") >= 0, "раскладка не переключилась");
}
END_TEST

START_TEST(er_keyboard_port_swaps_keymaps) {
    er_keyboard_layout();
    keyboard_port_ensure();

    const char* keys = kbd_port.keymap(&kbd_port);
    const char* saved = kbd_port.keymap_saved(&kbd_port);
    ck_assert_ptr_nonnull((void*) keys);
    ck_assert_ptr_nonnull((void*) saved);

    kbd_port.swap_keymap(&kbd_port);
    ck_assert_ptr_eq((void*) kbd_port.keymap(&kbd_port), (void*) saved);
    ck_assert_ptr_eq((void*) kbd_port.keymap_saved(&kbd_port), (void*) keys);
}
END_TEST

START_TEST(er_keyboard_port_clear_recover_is_repeatable) {
    er_keyboard_layout();
    mock_modifiers_fill();
    keyboard_port_ensure();

    // clear_active_mods() и recover_active_mods() должны быть парными:
    // после recover счётчик вложенности обязан вернуться в 0, иначе второй
    // перевод/инверсия регистра в процессе упадёт с ABORT.
    kbd_port.clear_active_mods(&kbd_port);
    ck_assert_int_eq(er_kbd.active_use, 1);
    kbd_port.recover_active_mods(&kbd_port);
    ck_assert_int_eq(er_kbd.active_use, 0);

    kbd_port.clear_active_mods(&kbd_port);
    ck_assert_int_eq(er_kbd.active_use, 1);
    kbd_port.recover_active_mods(&kbd_port);
    ck_assert_int_eq(er_kbd.active_use, 0);
}
END_TEST

START_TEST(er_keyboard_port_polls_keymaps) {
    er_keyboard_layout();
    keyboard_port_ensure();
    mock_reset();

    kbd_port.poll(&kbd_port);
    kbd_port.poll_saved(&kbd_port);

    // Опрос X11 остаётся внутри адаптера, приложение зовёт только порт.
    ck_assert_int_eq(mock_count("XQueryKeymap("), 2);
}
END_TEST

START_TEST(er_keyboard_port_presses_key) {
    er_keyboard_layout();
    keyboard_port_ensure();
    mock_reset();

    unikey_t key = {code : KEY_A, mods : 0, group : 0};
    kbd_port.press_key(&kbd_port, key);

    // Одно нажатие и одно отжатие.
    ck_assert_int_eq(mock_count("XTestFakeKeyEvent("), 2);
}
END_TEST

Suite* suite_keyboard() {
    Suite* s = suite_create("keyboard port");

    TCase* tc = tcase_create("x11 adapter");
    tcase_add_checked_fixture(tc, er_fixture, er_fixture_teardown);
    tcase_add_test(tc, er_keyboard_port_converts_symbols);
    tcase_add_test(tc, er_keyboard_port_classifies_keys);
    tcase_add_test(tc, er_keyboard_port_state_and_layout);
    tcase_add_test(tc, er_keyboard_port_swaps_keymaps);
    tcase_add_test(tc, er_keyboard_port_clear_recover_is_repeatable);
    tcase_add_test(tc, er_keyboard_port_polls_keymaps);
    tcase_add_test(tc, er_keyboard_port_presses_key);
    suite_add_tcase(s, tc);

    return s;
}
