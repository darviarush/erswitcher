#include "test.h"

/*
 * [sendkeys] — шаблоны, вводимые эмуляцией нажатий.
 *
 * sendkeys() печатает строку посимвольно: каждый utf32-символ ищется
 * по раскладке (keysym_to_unikey) и «нажимается» через press_key().
 * Вместо настоящего нажатия мок вызывает mock_fake_key_hook — тест
 * собирает сканкоды, и получается «эхо» ввода без иксов.
 *
 * Раскладку тест задаёт сам (mock_keymap_set), модификаторы и локи
 * добавляет mock_modifiers_fill() — без них sendkeys() спотыкается
 * во clear_active_mods()/send_mods().
 */

static int er_keys[64];
static int er_keys_n = 0;

// Перехват эмуляции нажатий: только сами клавиши, без модификаторов
static int er_record_key(Display* dpy, unsigned int code, Bool is_press, unsigned long fake_delay) {
    (void) dpy;
    (void) fake_delay;
    if(is_press && er_keys_n < 64) er_keys[er_keys_n++] = code;
    return 1;
}

// Раскладка: группа 0 — латиница a, b, c; группа 1 — кириллица а, б, в
// Одна и та же буква есть только в одной группе
static void er_layout() {
    mock_keymap_clear();
    mock_keymap_set(KEY_A, 0, 0, XK_a);
    mock_keymap_set(KEY_B, 0, 0, XK_b);
    mock_keymap_set(KEY_C, 0, 0, XK_c);
    mock_keymap_set(KEY_A, 1, 0, 0x06C1); // а
    mock_keymap_set(KEY_B, 1, 0, 0x06C2); // б
    mock_keymap_set(KEY_C, 1, 0, 0x06C3); // в

    mock_keymap_set_all_groups(KEY_LEFTSHIFT, XK_Shift_L);
    mock_keymap_set_all_groups(KEY_LEFTCTRL, XK_Control_L);
    mock_keymap_set_all_groups(KEY_CAPSLOCK, XK_Caps_Lock);
    mock_keymap_set_all_groups(KEY_NUMLOCK, XK_Num_Lock);
    mock_keymap_set_all_groups(KEY_LEFTALT, XK_Alt_L);
    mock_keymap_set_all_groups(KEY_LEFTMETA, XK_Meta_L);
    mock_modifiers_fill();

    er_kbd.groups = 2;
    er_kbd.group_en = 0;
    er_kbd.group_ru = 1;
    er_kbd.maybe_group = 0;
    mock_keyboard(0);

    er_keys_n = 0;
    mock_reset();
}


// @category шаблоны

START_TEST(er_sendkeys_presses_each_symbol) {
    er_layout();
    mock_fake_key_hook = er_record_key;

    sendkeys("abc");

    // Каждому символу — своё нажатие, ровно его сканкод
    // (KEY_A, KEY_B, KEY_C в Linux — не подряд идущие номера)
    int expected[] = {KEY_A, KEY_B, KEY_C};
    ck_assert_int_eq(er_keys_n, 3);
    for(int i = 0; i < er_keys_n; i++) {
        ck_assert_msg(er_keys[i] == expected[i], "нажатие %i: %i вместо %i", i, er_keys[i], expected[i]);
    }
    ck_assert_int_eq(mock_find("XkbLockGroup("), -1);
}
END_TEST

START_TEST(er_sendkeys_switches_layout_for_cyrillic) {
    er_layout();
    mock_fake_key_hook = er_record_key;

    // Кириллица есть только в группе 1 — sendkeys должен её включить
    sendkeys("аб");

    ck_assert_int_eq(er_keys_n, 2);
    ck_assert_msg(mock_find("XkbLockGroup(") >= 0, "раскладка не переключилась");
}
END_TEST

START_TEST(er_sendkeys_empty_string) {
    er_layout();
    mock_fake_key_hook = er_record_key;

    sendkeys("");

    ck_assert_int_eq(er_keys_n, 0);
}
END_TEST

START_TEST(er_sendkeys_missing_symbol_is_skipped) {
    er_layout();
    mock_fake_key_hook = er_record_key;

    // «ж» нет ни в одной раскладке — нажатия для неё не будет
    sendkeys("aжb");

    ck_assert_int_eq(er_keys_n, 2);
}
END_TEST

// @category разбор

START_TEST(er_sendkeys_from_config) {
    er_load(
        "[sendkeys]\n"
        "Pause=abc\n");

    ck_assert_int_eq(keyfn_size, 1);
    ck_assert_int_eq(keyfn[0].action, CONFIG_ACTION_SENDKEYS);
    ck_assert_str_eq(keyfn[0].arg, "abc");
}
END_TEST

START_TEST(er_sendkeys_escapes) {
    er_load(
        "[sendkeys]\n"
        "Pause=Готово.\\n\\tКонец\n");

    ck_assert_int_eq(keyfn_size, 1);
    ck_assert_str_eq(keyfn[0].arg, "Готово.\n\tКонец");
}
END_TEST

Suite* suite_sendkeys() {
    Suite* s = suite_create("[sendkeys]");

    TCase* tc = tcase_create("ввод");
    tcase_add_checked_fixture(tc, er_fixture, er_fixture_teardown);
    tcase_add_test(tc, er_sendkeys_presses_each_symbol);
    tcase_add_test(tc, er_sendkeys_switches_layout_for_cyrillic);
    tcase_add_test(tc, er_sendkeys_empty_string);
    tcase_add_test(tc, er_sendkeys_missing_symbol_is_skipped);
    tcase_add_test(tc, er_sendkeys_from_config);
    tcase_add_test(tc, er_sendkeys_escapes);
    suite_add_tcase(s, tc);

    return s;
}
