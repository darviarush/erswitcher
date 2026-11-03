#include <linux/input-event-codes.h>
#include "test.h"

/*
 * [compose] — мнемоники.
 *
 * Проверяется всё, что не требует живого X-сервера:
 *
 *  1. разбор: ключ -> compose_map[].word/pos, подстановки \n, \t, \\, \^
 *     (cursor_back), сортировка по убыванию длины ключа;
 *  2. выбор мнемоники: compose() читает буфер ввода (pos, word[]) и по
 *     KEY_TO_INT (по символу) находит самый длинный подходящий ключ;
 *  3. вставка: что ушло в буфер обмена и сколько бэкспейсов отправлено.
 *
 * Раскладка — та же схема, что на настоящей клавиатуре: буквы в двух группах
 * (латиница/кириллица), Tab и BackSpace — в обеих, оба уровня Shift.
 * Без Tab и BackSpace в таблице keysym_to_unikey() путает буквы с клавишей Tab
 * (XK_Tab == 0xFF09 == 65545, а это больше KEYBOARD_SIZE).
 */

#define CODE_A KEY_A
#define CODE_B KEY_B
#define CODE_C KEY_C

static void er_layout_abc() {
    mock_keymap_clear();
    mock_keymap_set(CODE_A, 0, 0, XK_a);
    mock_keymap_set(CODE_B, 0, 0, XK_b);
    mock_keymap_set(CODE_C, 0, 0, XK_c);
    mock_keymap_set_all_groups(KEY_TAB, XK_Tab);
    mock_keymap_set_all_groups(KEY_BACKSPACE, XK_BackSpace);
    mock_keymap_set_all_groups(KEY_LEFT, XK_Left);
    mock_keymap_set_all_groups(KEY_LEFTSHIFT, XK_Shift_L);
    mock_keymap_set_all_groups(KEY_LEFTCTRL, XK_Control_L);
    mock_keymap_set_all_groups(KEY_LEFTALT, XK_Alt_L);
    mock_keymap_set_all_groups(KEY_LEFTMETA, XK_Meta_L);
    mock_keymap_set_all_groups(KEY_CAPSLOCK, XK_Caps_Lock);
    mock_keymap_set_all_groups(KEY_NUMLOCK, XK_Num_Lock);
    mock_modifiers_fill();
    groups = 1;
    group_en = 0;
    group_ru = -1;
    maybe_group = 0;
    mock_keyboard(0);
}

static unikey_t er_key(char ch) {
    return (unikey_t) {code: ch - 'a' + CODE_A, mods: 0, group: 0};
}

static void er_input(char const* s) {
    for(; *s; s++) word[pos++] = er_key(*s);
}


// @category разбор

START_TEST(er_compose_single_letter) {
    er_load(
        "[compose]\n"
        "a=б\n"
        "b=в\n"
    );

    ck_assert_int_eq(compose_map_size, 2);
    ck_assert_str_eq(compose_map[0].to, "б");
    ck_assert_int_eq(compose_map[0].pos, 1);
    ck_assert_int_eq(compose_map[0].cursor_back, 0);
    ck_assert_str_eq(compose_map[1].to, "в");
}
END_TEST

START_TEST(er_compose_two_letter) {
    er_load(
        "[compose]\n"
        "ab=аб\n"
    );

    ck_assert_int_eq(compose_map_size, 1);
    ck_assert_int_eq(compose_map[0].pos, 2);
    ck_assert_int_eq(compose_map[0].word[0], L'a');
    ck_assert_int_eq(compose_map[0].word[1], L'b');
    ck_assert_str_eq(compose_map[0].to, "аб");
}
END_TEST

START_TEST(er_compose_cyrillic_key_is_utf32) {
    er_load(
        "[compose]\n"
        "ы=і\n"
    );

    ck_assert_int_eq(compose_map_size, 1);
    ck_assert_int_eq(compose_map[0].pos, 1);
    ck_assert_int_eq(compose_map[0].word[0], (wint_t) 0x044B);
    ck_assert_str_eq(compose_map[0].to, "і");
}
END_TEST

START_TEST(er_compose_sorted_by_key_length) {
    er_load(
        "[compose]\n"
        "a=1\n"
        "ab=2\n"
        "abc=3\n"
    );

    ck_assert_int_eq(compose_map_size, 3);
    ck_assert_int_eq(compose_map[0].pos, 3);
    ck_assert_int_eq(compose_map[1].pos, 2);
    ck_assert_int_eq(compose_map[2].pos, 1);
}
END_TEST

START_TEST(er_compose_escaped_equal_in_key) {
    // \== — ключ «=», а не «\»
    er_load(
        "[compose]\n"
        "\\==±\n"
        "<\\==≤\n"
    );

    ck_assert_int_eq(compose_map_size, 2);
    ck_assert_int_eq(compose_map[0].word[0], L'<');
    ck_assert_int_eq(compose_map[0].word[1], L'=');
    ck_assert_int_eq(compose_map[0].pos, 2);
    ck_assert_str_eq(compose_map[0].to, "≤");
}
END_TEST

START_TEST(er_compose_cursor_back) {
    er_load(
        "[compose]\n"
        "if=if(\\^) {\\n\\t\\n}\n"
    );

    ck_assert_int_eq(compose_map_size, 1);
    // После «if(» одна позиция, дальше «) {\n\t\n}» — 8 символов
    ck_assert_int_eq(compose_map[0].cursor_back, 8);
    ck_assert_str_eq(compose_map[0].to, "if() {\n\t\n}");
}
END_TEST

START_TEST(er_compose_escapes) {
    er_load(
        "[compose]\n"
        "pr=print \\^, \"\\\\n\";\n"
        "t=a\\tb\n"
        "n=a\\nb\n"
    );

    ck_assert_int_eq(compose_map_size, 3);
    ck_assert_str_eq(compose_map[0].to, "print , \"\\n\";");
    ck_assert_int_eq(compose_map[0].cursor_back, 6);
    ck_assert_str_eq(compose_map[1].to, "a\tb");
    ck_assert_str_eq(compose_map[2].to, "a\nb");
}
END_TEST

START_TEST(er_compose_unknown_escape_kept) {
    er_load("[compose]\na=\\x\n");

    ck_assert_int_eq(compose_map_size, 1);
    ck_assert_str_eq(compose_map[0].to, "\\x");
}
END_TEST

START_TEST(er_compose_too_long_key_is_skipped) {
    // COMPOSE_KEY_MAX = 20 символов, 21 — ошибка в конфиге
    er_load(
        "[compose]\n"
        "aaaaaaaaaaaaaaaaaaaaa=длинно\n"
        "a=коротко\n"
    );

    ck_assert_int_eq(compose_map_size, 1);
    ck_assert_str_eq(compose_map[0].to, "коротко");
}
END_TEST

START_TEST(er_compose_empty_value_allowed) {
    er_load("[compose]\nab=\n");

    ck_assert_int_eq(compose_map_size, 1);
    ck_assert_int_eq(compose_map[0].pos, 2);
    ck_assert_str_eq(compose_map[0].to, "");
}
END_TEST

START_TEST(er_compose_reload_drops_old_map) {
    er_load("[compose]\na=1\nb=2\n");
    ck_assert_int_eq(compose_map_size, 2);

    er_load("[compose]\nc=3\n");
    ck_assert_int_eq(compose_map_size, 1);
    ck_assert_str_eq(compose_map[0].to, "3");
}
END_TEST

// @category выбор мнемоники и вставка

START_TEST(er_compose_applies_longest_match) {
    er_layout_abc();
    er_load(
        "[compose]\n"
        "a=один\n"
        "ab=два\n"
        "abc=три\n"
    );

    er_input("abc");
    mock_reset();
    compose(NULL);

    ck_assert_ptr_nonnull((void*) insert_from_clipboard_data);
    ck_assert_str_eq(insert_from_clipboard_data, "три");
    // Удаляем ровно столько символов, сколько в ключе
    ck_assert_int_eq(mock_count("XTestFakeKeyEvent("), 3);
}
END_TEST

START_TEST(er_compose_no_match_does_nothing) {
    er_layout_abc();
    er_load("[compose]\nxyz=нет\n");

    er_input("abc");
    mock_reset();
    compose(NULL);

    ck_assert_ptr_null((void*) insert_from_clipboard_data);
    ck_assert_int_eq(mock_find("XTestFakeKeyEvent("), -1);
}
END_TEST

START_TEST(er_compose_empty_buffer_does_nothing) {
    er_layout_abc();
    er_load("[compose]\na=б\n");

    mock_reset();
    compose(NULL);

    ck_assert_ptr_null((void*) insert_from_clipboard_data);
    ck_assert_int_eq(mock_n, 0);
}
END_TEST

START_TEST(er_compose_partial_key_matches_head) {
    er_layout_abc();
    er_load("[compose]\naa=б\n");

    // В буфере «ba»: ключу «aa» не хватает первого символа
    er_input("ba");
    mock_reset();
    compose(NULL);

    ck_assert_ptr_null((void*) insert_from_clipboard_data);
}
END_TEST

START_TEST(er_compose_key_after_other_symbols) {
    er_layout_abc();
    er_load("[compose]\nab=два\n");

    er_input("xab");
    mock_reset();
    compose(NULL);

    ck_assert_str_eq(insert_from_clipboard_data, "два");
}
END_TEST

START_TEST(er_compose_key_with_switched_layout) {
    // «a» есть только в группе 1, а активна 0:
    // keysym_to_unikey() должен найти её перебором групп
    mock_keymap_clear();
    mock_keymap_set(CODE_A, 1, 0, XK_a);
    mock_keymap_set_all_groups(KEY_TAB, XK_Tab);
    mock_keymap_set_all_groups(KEY_BACKSPACE, XK_BackSpace);
    mock_keymap_set_all_groups(KEY_LEFTSHIFT, XK_Shift_L);
    mock_keymap_set_all_groups(KEY_LEFTCTRL, XK_Control_L);
    mock_keymap_set_all_groups(KEY_LEFTALT, XK_Alt_L);
    mock_keymap_set_all_groups(KEY_LEFTMETA, XK_Meta_L);
    mock_keymap_set_all_groups(KEY_CAPSLOCK, XK_Caps_Lock);
    mock_keymap_set_all_groups(KEY_NUMLOCK, XK_Num_Lock);
    mock_modifiers_fill();
    groups = 2;
    group_en = 0;
    group_ru = 1;
    maybe_group = 0;

    er_load("[compose]\na=б\n");

    word[pos++] = (unikey_t) {code: CODE_A, mods: 0, group: 1};
    mock_reset();
    compose(NULL);

    ck_assert_str_eq(insert_from_clipboard_data, "б");
    ck_assert_msg(mock_find("XkbLockGroup(") >= 0, "группа не переключилась");
}
END_TEST

Suite* suite_compose() {
    Suite* s = suite_create("[compose]");

    TCase* parse = tcase_create("разбор");
    tcase_add_checked_fixture(parse, er_fixture, er_fixture_teardown);
    tcase_add_test(parse, er_compose_single_letter);
    tcase_add_test(parse, er_compose_two_letter);
    tcase_add_test(parse, er_compose_cyrillic_key_is_utf32);
    tcase_add_test(parse, er_compose_sorted_by_key_length);
    tcase_add_test(parse, er_compose_escaped_equal_in_key);
    tcase_add_test(parse, er_compose_cursor_back);
    tcase_add_test(parse, er_compose_escapes);
    tcase_add_test(parse, er_compose_unknown_escape_kept);
    tcase_add_test(parse, er_compose_too_long_key_is_skipped);
    tcase_add_test(parse, er_compose_empty_value_allowed);
    tcase_add_test(parse, er_compose_reload_drops_old_map);
    suite_add_tcase(s, parse);

    TCase* apply = tcase_create("применение");
    tcase_add_checked_fixture(apply, er_fixture, er_fixture_teardown);
    tcase_add_test(apply, er_compose_applies_longest_match);
    tcase_add_test(apply, er_compose_no_match_does_nothing);
    tcase_add_test(apply, er_compose_empty_buffer_does_nothing);
    tcase_add_test(apply, er_compose_partial_key_matches_head);
    tcase_add_test(apply, er_compose_key_after_other_symbols);
    tcase_add_test(apply, er_compose_key_with_switched_layout);
    suite_add_tcase(s, apply);

    return s;
}
