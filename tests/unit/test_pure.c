#include <linux/input-event-codes.h>
#include "test.h"

/*
 * Чистые функции, которые не привязаны к конкретной секции конфига.
 * Когда они начнут переезжать в src/, здесь поменяются только #include.
 */

// Символ кладётся в word[] как клавиша по таблице мока: KEY_TO_INT()
// достаёт из неё utf32-символ, по которому и работают iswspace/печать
static unikey_t pure_key(int ch) {
    int code = ch == ' ' ? KEY_SPACE : KEY_A + (ch - 'a');
    return (unikey_t) {code: code, mods: 0, group: 0};
}

// Раскладка для буфера ввода: буквы и пробел
static void pure_layout() {
    mock_keymap_clear();
    mock_keymap_set(KEY_A, 0, 0, XK_a);
    mock_keymap_set(KEY_B, 0, 0, XK_b);
    mock_keymap_set(KEY_C, 0, 0, XK_c);
    mock_keymap_set_all_groups(KEY_SPACE, XK_space);
    mock_modifiers_fill();
    groups = 1;
    maybe_group = 0;
}

// @category utf8

START_TEST(er_utf8_strlen_ascii) {
    ck_assert_int_eq(utf8_strlen(""), 0);
    ck_assert_int_eq(utf8_strlen("abc"), 3);
}
END_TEST

START_TEST(er_utf8_strlen_cyrillic) {
    ck_assert_int_eq(utf8_strlen("привет"), 6);
    ck_assert_int_eq(utf8_strlen("Ёж"), 2);
}
END_TEST

START_TEST(er_utf8_strlen_mixed_and_emoji) {
    ck_assert_int_eq(utf8_strlen("abcы"), 4);
    ck_assert_int_eq(utf8_strlen("😀"), 1);
    ck_assert_int_eq(utf8_strlen("a😀b"), 3);
}
END_TEST

// @category буфер ввода

START_TEST(er_from_space_tail_word) {
    pure_layout();
    // «abc xyz»: последнее слово начинается после пробела
    char const* s = "abc xyz";
    for(int i = 0; s[i]; i++) word[i] = pure_key(s[i]);
    pos = strlen(s);
    ck_assert_int_eq(from_space(), 4);
}
END_TEST

START_TEST(er_from_space_single_word) {
    pure_layout();
    for(int i = 0; i < 3; i++) word[i] = pure_key('a' + i);
    pos = 3;
    ck_assert_int_eq(from_space(), 0);
}
END_TEST

START_TEST(er_from_space_empty_buffer) {
    pos = 0;
    ck_assert_int_eq(from_space(), 0);
}
END_TEST

// @category поиск в списке

START_TEST(er_in_sym_sequence) {
    KeySym syms[] = {XK_a, XK_b, 0};
    ck_assert_int_eq(in_sym(XK_a, syms), 1);
    ck_assert_int_eq(in_sym(XK_b, syms), 1);
    ck_assert_int_eq(in_sym(XK_c, syms), 0);
}
END_TEST

START_TEST(er_in_sym_empty) {
    KeySym syms[] = {0};
    ck_assert_int_eq(in_sym(XK_a, syms), 0);
}
END_TEST

// @category прочее

START_TEST(er_get_event_type_names) {
    ck_assert_str_eq(get_event_type(KeyPress), "KeyPress");
    ck_assert_str_eq(get_event_type(SelectionNotify), "SelectionNotify");
    ck_assert_str_eq(get_event_type(LASTEvent), "LASTEvent");
    ck_assert_ptr_null(get_event_type(0));
    ck_assert_ptr_null(get_event_type(LASTEvent + 1));
}
END_TEST

Suite* suite_pure() {
    Suite* s = suite_create("чистые функции");

    TCase* tc = tcase_create("без конфига");
    tcase_add_checked_fixture(tc, er_fixture, er_fixture_teardown);
    tcase_add_test(tc, er_utf8_strlen_ascii);
    tcase_add_test(tc, er_utf8_strlen_cyrillic);
    tcase_add_test(tc, er_utf8_strlen_mixed_and_emoji);
    tcase_add_test(tc, er_from_space_tail_word);
    tcase_add_test(tc, er_from_space_single_word);
    tcase_add_test(tc, er_from_space_empty_buffer);
    tcase_add_test(tc, er_in_sym_sequence);
    tcase_add_test(tc, er_in_sym_empty);
    tcase_add_test(tc, er_get_event_type_names);
    suite_add_tcase(s, tc);

    return s;
}
